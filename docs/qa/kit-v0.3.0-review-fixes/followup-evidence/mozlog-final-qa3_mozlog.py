# SPDX-License-Identifier: MPL-2.0

import re

from runtime_build_context import parse_rest_json

CANARY_REASON = "QA3_EXPECTED_ASSERTION"
MAX_EVENT_BYTES = 1024**2


def canonical_test(value, suite, package):
    if not isinstance(value, str):
        raise ValueError("non-string Mozilla test ID")
    root = package / (
        "mochitest/browser" if suite == "browser-chrome" else "xpcshell/tests"
    )
    prefixes = [str(root.resolve()) + "/"]
    if suite == "browser-chrome":
        prefixes.append("chrome://mochitests/content/browser/")
    for prefix in prefixes:
        if value.startswith(prefix):
            value = value[len(prefix) :]
            break
    from qa3_source_cohort import safe_path

    return safe_path(value)


def _tests(value):
    if isinstance(value, list):
        return value
    if isinstance(value, dict) and all(isinstance(v, list) for v in value.values()):
        return [item for group in value.values() for item in group]
    raise ValueError("missing suite-start selection")


def verify_mozlog(path, case, package, process, canary=False):
    if (
        path.is_symlink()
        or not path.is_file()
        or not 0 < path.stat().st_size <= 2 * 1024**3
    ):
        raise ValueError("missing or oversized complete raw mozlog")
    if process["stop"] or not process["ownedCleanupComplete"]:
        raise ValueError("incomplete native lifecycle")
    if canary:
        if type(process["exit"]) is not int or process["exit"] != 1:
            raise ValueError("assertion canary did not fail normally")
    elif process["exit"] != 0:
        raise ValueError("native suite exited unsuccessfully")
    suite_started = False
    suite_ended = False
    test_started = False
    test_ended = False
    assertions = 0
    canary_hits = 0
    events = 0
    case_id = case["id"]
    with path.open("rb") as stream:
        while line := stream.readline(MAX_EVENT_BYTES + 1):
            events += 1
            if len(line) > MAX_EVENT_BYTES or events > 1000000:
                raise ValueError("raw mozlog event budget exceeded")
            if not line.endswith(b"\n"):
                raise ValueError("unterminated raw mozlog event")
            entry = parse_rest_json(line)
            action = entry.get("action")
            if not isinstance(action, str):
                raise ValueError("untyped Mozilla event")
            if suite_ended:
                if action not in {"log", "process_output"}:
                    raise ValueError("execution event after suite end")
            if action == "suite_start":
                ids = [
                    canonical_test(v, case["suite"], package)
                    for v in _tests(entry.get("tests"))
                ]
                if suite_started or ids != [case_id]:
                    raise ValueError("empty, extra, duplicate or wrong suite selection")
                suite_started = True
            elif action == "suite_end":
                if not suite_started or suite_ended or not test_ended:
                    raise ValueError("suite ended without complete required test")
                suite_ended = True
            elif action in {"test_start", "test_status", "test_end"}:
                if not suite_started or suite_ended:
                    raise ValueError("test event outside suite lifecycle")
                if canonical_test(entry.get("test"), case["suite"], package) != case_id:
                    raise ValueError("unknown or foreign-source test ID")
                if action == "test_start":
                    if test_started:
                        raise ValueError("duplicate execution or hidden retry")
                    test_started = True
                elif action == "test_status":
                    if not test_started or test_ended:
                        raise ValueError("assertion outside required test")
                    status = entry.get("status")
                    reason = entry.get("subtest")
                    if "subtest" not in entry or (
                        reason is not None and not isinstance(reason, str)
                    ):
                        raise ValueError("missing or untyped assertion subtest")
                    if (
                        canary
                        and status == "FAIL"
                        and entry.get("expected") == "PASS"
                        and isinstance(reason, str)
                        and re.fullmatch(r"QA3_EXPECTED_ASSERTION(?: - .*)?", reason)
                    ):
                        canary_hits += 1
                    elif status != "PASS" or entry.get("expected", "PASS") != "PASS":
                        raise ValueError(
                            "unexpected assertion, required skip or recovered first failure"
                        )
                    assertions += 1
                else:
                    if not test_started or test_ended:
                        raise ValueError("missing start or duplicate test completion")
                    if entry.get("status") not in (
                        {"OK", "PASS", "FAIL"} if canary else {"OK", "PASS"}
                    ):
                        raise ValueError(
                            "skip, crash, timeout or unsuccessful test end"
                        )
                    if (
                        not canary
                        and entry.get("expected", entry["status"]) != entry["status"]
                    ):
                        raise ValueError("unexpected terminal test status")
                    test_ended = True
            elif action in {"crash", "assertion_count"}:
                if (
                    action == "crash"
                    or type(entry.get("count")) is not int
                    or entry["count"] != 0
                ):
                    raise ValueError("native crash or assertion-count failure")
            elif action in {"lsan_leak", "mozleak_object"}:
                raise ValueError("native leak evidence")
            elif action == "mozleak_total":
                if type(entry.get("bytes")) is not int or entry["bytes"] != 0:
                    raise ValueError("native leak total is missing or nonzero")
            elif action == "process_exit":
                if type(entry.get("exitcode")) is not int or entry["exitcode"] != 0:
                    raise ValueError("unexpected native child exit")
            elif action == "log":
                if entry.get("level") in {"ERROR", "CRITICAL", "FATAL"}:
                    raise ValueError("harness error is not assertion-canary success")
                message = entry.get("message", "")
                if isinstance(message, str) and re.search(
                    r"(?i)retry|rerun|TEST-UNEXPECTED", message
                ):
                    raise ValueError("first failure or internal retry recorded")
            elif action not in {
                "process_output",
                "process_start",
                "group_start",
                "group_end",
                "shutdown",
            }:
                raise ValueError("unrecognized Mozilla execution event")
    if not suite_started or not suite_ended or not test_started or not test_ended:
        raise ValueError("required execution missing or incomplete")
    if assertions < case["minimumAssertions"] or canary and canary_hits != 1:
        raise ValueError(
            "assertion-free execution or wrong canary phase/reason/event ID"
        )
    return {
        "id": case_id,
        "suite": case["suite"],
        "assertions": assertions,
        "rawEvents": events,
        "verdict": "EXPECTED_REJECTION" if canary else "PASS",
        "canaryPhase": "test_status" if canary else None,
        "canaryReason": CANARY_REASON if canary else None,
    }
