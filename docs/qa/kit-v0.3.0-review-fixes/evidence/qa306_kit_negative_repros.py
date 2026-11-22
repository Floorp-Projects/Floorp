"""Synthetic review reproductions; no browser, network or repository changes."""

import hashlib
import json
import os
from pathlib import Path
import resource
import signal
import subprocess
import sys
import tempfile
import time

EVIDENCE = Path(__file__).resolve().parent
KIT = EVIDENCE.parent / "inputs/github-87311566/extracted/Floorp_QA_Implementation_Kit_v0.3.0"
sys.path.insert(0, str(KIT / "tools/qa"))
sys.path.insert(0, str(KIT / "tools/qa/tests"))


def child(mode):
    if mode == "attempt":
        import test_core

        fixture = test_core.KitTest()
        fixture.setUp()
        try:
            fixture.records[0]["attempt"] = 2**53 - 1
            try:
                verdict = fixture.evaluate()
                print(json.dumps({"kind": "verdict", "value": verdict}))
            except (MemoryError, OverflowError) as error:
                print(json.dumps({"kind": "resource-exception", "type": type(error).__name__}))
        finally:
            fixture.doCleanups()
    elif mode == "agent-partial":
        import agent

        class FakeDriver:
            def __init__(self, repo, browser, output, subject):
                output.mkdir()

            def observe(self):
                return {"synthetic": True}

            def execute(self, action):
                return self.observe()

            def close(self):
                pass

        agent.Driver = FakeDriver
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            value = agent.execute_campaign(
                root, root / "unused", {"id": "synthetic-review"},
                root / "out", None, 2, 1,
            )
            print(json.dumps(value))
    else:
        raise ValueError("Unsupported review mode")


def limits():
    resource.setrlimit(resource.RLIMIT_AS, (256 * 1024**2, 256 * 1024**2))
    resource.setrlimit(resource.RLIMIT_CPU, (8, 8))
    resource.setrlimit(resource.RLIMIT_FSIZE, (8 * 1024**2, 8 * 1024**2))


def main():
    import test_core

    fixture = test_core.KitTest()
    fixture.setUp()
    cases = {}
    try:
        def check(name, events):
            value = fixture.log(events)
            path = EVIDENCE / ("qa306-mozlog-" + name + ".jsonl")
            with path.open("xb") as stream:
                stream.write((fixture.root / "mozlog.jsonl").read_bytes())
            value["retainedRawPath"] = path.name
            return value

        good = fixture.happy_events()
        cases["zero-assertions"] = check("zero-assertions", [good[0], good[1], good[3], good[4]])
        for name, event in {
            "retry-warning": {"action": "log", "level": "WARNING", "message": "Retrying failed test after first failure"},
            "failed-child-process": {"action": "process_exit", "process": "synthetic-child", "exitcode": 1},
            "unknown-action": {"action": "unsupported_required_diagnostic"},
        }.items():
            cases[name] = check(name, good[:3] + [event] + good[3:])
    finally:
        fixture.doCleanups()

    env = {
        "PATH": os.environ["PATH"], "LANG": "C.UTF-8", "PYTHONDONTWRITEBYTECODE": "1",
        "TMPDIR": str(EVIDENCE / "qa306-test-temp"),
        "GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_GLOBAL": "/dev/null", "GIT_TERMINAL_PROMPT": "0",
    }
    command = [sys.executable, "-B", str(Path(__file__).resolve())]
    attempt = subprocess.run(command + ["attempt"], env=env, capture_output=True,
                             text=True, timeout=10, preexec_fn=limits)
    cases["huge-attempt"] = {"exitCode": attempt.returncode, "stdout": attempt.stdout, "stderr": attempt.stderr}

    start = time.monotonic()
    log = EVIDENCE / "qa306-agent-partial-stdin.log"
    with log.open("xb") as stream:
        proc = subprocess.Popen(command + ["agent-partial"], env=env, stdin=subprocess.PIPE,
                                stdout=stream, stderr=subprocess.STDOUT, start_new_session=True,
                                preexec_fn=limits)
        proc.stdin.write(b"{")
        proc.stdin.flush()
        exceeded = False
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            exceeded = True
            os.killpg(proc.pid, signal.SIGKILL)
            proc.wait(timeout=3)
        finally:
            proc.stdin.close()
    cases["agent-partial-stdin"] = {
        "campaignBudgetSeconds": 1, "outerTimeoutSeconds": 3,
        "exceededCampaignBudget": exceeded, "wallSeconds": round(time.monotonic() - start, 3),
        "exitCodeAfterOuterCleanup": proc.returncode,
        "fakeDriver": True, "processGroupCleanup": "owned child only",
        "logSha256": hashlib.sha256(log.read_bytes()).hexdigest(),
    }
    value = {
        "schemaVersion": 1, "sourceZipSha256": "a4af60380399050f82465864734391227ab34024d26bdf16c31559e2824a8d50",
        "realBrowserStarted": False, "nativeBuildExecuted": False, "networkCalled": False,
        "repoFilesMutated": False, "resourceCaps": {"addressSpaceMiB": 256, "cpuSeconds": 8, "fileMiB": 8},
        "cases": cases,
    }
    output = EVIDENCE / "qa306-kit-negative-repros.json"
    with output.open("x") as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(json.dumps({name: item.get("status", item) for name, item in cases.items()}, ensure_ascii=False))


if __name__ == "__main__":
    if len(sys.argv) == 2:
        child(sys.argv[1])
    else:
        main()
