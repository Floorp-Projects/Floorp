# SPDX-License-Identifier: MPL-2.0

import copy
import json
import tempfile
import unittest
from pathlib import Path

from qa3_mozlog import verify_mozlog


class MozlogContracts(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.path = self.root / "raw.jsonl"
        self.case = {"suite": "xpcshell", "id": "xpcom/test.js", "minimumAssertions": 1}
        self.process = {"exit": 0, "stop": None, "ownedCleanupComplete": True}
        self.events = [
            {"action": "suite_start", "tests": ["xpcom/test.js"]},
            {"action": "test_start", "test": "xpcom/test.js"},
            {
                "action": "test_status",
                "test": "xpcom/test.js",
                "status": "PASS",
                "subtest": "assert",
            },
            {"action": "test_end", "test": "xpcom/test.js", "status": "OK"},
            {"action": "suite_end"},
        ]

    def verify(self, events=None, canary=False, process=None):
        self.path.write_text(
            "".join(
                json.dumps(v) + "\n"
                for v in (self.events if events is None else events)
            )
        )
        return verify_mozlog(
            self.path, self.case, self.root, process or self.process, canary=canary
        )

    def test_exact_complete_required_execution(self):
        result = self.verify()
        self.assertEqual(result["verdict"], "PASS")
        self.assertEqual(result["assertions"], 1)

    def test_canonical_package_id_and_browser_chrome_uri(self):
        for suite, test in (
            ("xpcshell", str(self.root / "xpcshell/tests/xpcom/test.js")),
            ("browser-chrome", "chrome://mochitests/content/browser/xpcom/test.js"),
        ):
            self.case["suite"] = suite
            events = copy.deepcopy(self.events)
            events[0]["tests"] = [test]
            for event in events[1:4]:
                event["test"] = test
            self.assertEqual(self.verify(events)["verdict"], "PASS")

    def test_empty_incomplete_assertion_free_and_missing_case(self):
        for events in (
            [],
            self.events[:-1],
            self.events[1:],
            [self.events[0], *self.events[3:]],
            self.events[:2] + self.events[3:],
        ):
            with self.subTest(events=events), self.assertRaises(ValueError):
                self.verify(events)

    def test_required_skip_and_recovered_first_failure(self):
        for status in ("SKIP", "FAIL", "ERROR", "TIMEOUT"):
            events = copy.deepcopy(self.events)
            events[2]["status"] = status
            with self.subTest(status=status), self.assertRaises(ValueError):
                self.verify(events)
        events = copy.deepcopy(self.events)
        events.insert(2, {**events[2], "status": "FAIL"})
        with self.assertRaises(ValueError):
            self.verify(events)

    def test_first_failed_process_cannot_be_hidden_by_pass_log(self):
        for process in (
            {**self.process, "exit": 1},
            {**self.process, "stop": "TIMEOUT"},
            {**self.process, "ownedCleanupComplete": False},
        ):
            with self.subTest(process=process), self.assertRaises(ValueError):
                self.verify(process=process)

    def test_duplicate_execution_and_internal_retry(self):
        events = copy.deepcopy(self.events)
        events.insert(3, self.events[1])
        with self.assertRaises(ValueError):
            self.verify(events)
        for message in (
            "retrying after first failure",
            "rerun test",
            "TEST-UNEXPECTED-FAIL",
        ):
            events = [
                *self.events,
                {"action": "log", "level": "INFO", "message": message},
            ]
            with self.assertRaises(ValueError):
                self.verify(events)

    def test_extra_suite_foreign_source_and_duplicate_id(self):
        for tests in (
            [],
            ["other.js"],
            ["xpcom/test.js", "xpcom/test.js"],
            ["/other/checkout/xpcom/test.js"],
        ):
            events = copy.deepcopy(self.events)
            events[0]["tests"] = tests
            with self.subTest(tests=tests), self.assertRaises(ValueError):
                self.verify(events)

    def test_crash_leak_and_unknown_event(self):
        for event in (
            {"action": "crash"},
            {"action": "assertion_count", "count": 1},
            {"action": "lsan_leak", "frames": ["fixture"]},
            {"action": "mozleak_total", "bytes": 1},
            {"action": "unknown"},
        ):
            with self.subTest(event=event), self.assertRaises(ValueError):
                self.verify([*self.events[:3], event, *self.events[3:]])

    def test_child_process_exit_requires_typed_zero(self):
        event = {"action": "process_exit", "process": "fixture", "exitcode": 0}
        self.assertEqual(
            self.verify([*self.events[:3], event, *self.events[3:]])["verdict"],
            "PASS",
        )
        for invalid in (
            {},
            {"exitcode": False},
            {"exitcode": True},
            {"exitcode": 0.0},
            {"exitcode": 1.0},
            {"exitcode": None},
            {"exitcode": "0"},
            {"exitcode": 1},
        ):
            event = {"action": "process_exit", "process": "fixture", **invalid}
            with self.subTest(event=event), self.assertRaises(ValueError):
                self.verify([*self.events[:3], event, *self.events[3:]])

    def test_exact_expected_assertion_canary(self):
        events = copy.deepcopy(self.events)
        events[2].update(
            status="FAIL", expected="PASS", subtest="QA3_EXPECTED_ASSERTION"
        )
        result = self.verify(events, canary=True, process={**self.process, "exit": 1})
        self.assertEqual(result["verdict"], "EXPECTED_REJECTION")

    def test_unexecuted_setup_failure_is_not_expected_canary(self):
        for reason in ("unrelated assertion", "not QA3_EXPECTED_ASSERTION", None):
            events = copy.deepcopy(self.events)
            events[2].update(status="FAIL", subtest=reason)
            with self.subTest(reason=reason), self.assertRaises(ValueError):
                self.verify(events, canary=True, process={**self.process, "exit": 1})
        with self.assertRaises(ValueError):
            self.verify([], canary=True, process={**self.process, "exit": 1})
        with self.assertRaises(ValueError):
            self.verify(canary=True, process={**self.process, "exit": 1})

    def test_canary_cannot_accept_zero_exit_crash_or_two_expected_failures(self):
        events = copy.deepcopy(self.events)
        events[2].update(
            status="FAIL", expected="PASS", subtest="QA3_EXPECTED_ASSERTION"
        )
        for process in (self.process, {**self.process, "exit": -11}):
            with self.assertRaises(ValueError):
                self.verify(events, canary=True, process=process)
        events.insert(3, events[2])
        with self.assertRaises(ValueError):
            self.verify(events, canary=True, process={**self.process, "exit": 1})

    def test_canary_rejects_expected_fail_skip_retry_exit_and_untyped_assertions(self):
        events = copy.deepcopy(self.events)
        events[2].update(
            status="FAIL", expected="PASS", subtest="QA3_EXPECTED_ASSERTION"
        )
        for expected in ("FAIL", "SKIP", None):
            changed = copy.deepcopy(events)
            changed[2]["expected"] = expected
            with self.subTest(expected=expected), self.assertRaises(ValueError):
                self.verify(changed, canary=True, process={**self.process, "exit": 1})
        for code in (2, 3, 4, 5):
            with self.subTest(code=code), self.assertRaises(ValueError):
                self.verify(events, canary=True, process={**self.process, "exit": code})
        for count in (-1, None, True):
            with self.subTest(count=count), self.assertRaises(ValueError):
                self.verify([
                    *self.events[:3],
                    {"action": "assertion_count", "count": count},
                    *self.events[3:],
                ])

    def test_error_after_suite_and_truncated_raw_are_rejected(self):
        with self.assertRaises(ValueError):
            self.verify([
                *self.events,
                {"action": "log", "level": "ERROR", "message": "setup failed"},
            ])
        self.path.write_bytes(b'{"action":"suite_start"')
        with self.assertRaises(ValueError):
            verify_mozlog(self.path, self.case, self.root, self.process)


if __name__ == "__main__":
    unittest.main()
