# SPDX-License-Identifier: MPL-2.0

import copy
import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock

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

    def test_subtest_field_and_raw_type_are_required(self):
        for fields in (
            {},
            {"subtest": False},
            {"subtest": True},
            {"subtest": 0},
            {"subtest": 0.0},
            {"subtest": []},
            {"subtest": {}},
        ):
            events = copy.deepcopy(self.events)
            del events[2]["subtest"]
            events[2].update(fields)
            with self.subTest(fields=fields), self.assertRaisesRegex(
                ValueError, "subtest"
            ):
                self.verify(events)

    def test_formal_nullable_and_string_subtests_are_preserved(self):
        for value in (None, "", " \t", "assertion 雪"):
            events = copy.deepcopy(self.events)
            events[2]["subtest"] = value
            with self.subTest(value=value):
                result = self.verify(events)
                self.assertEqual(result["verdict"], "PASS")
                self.assertEqual(result["assertions"], 1)

    def test_readline_is_bounded_before_oversized_event_is_loaded(self):
        limit = 256
        data = json.dumps({"action": "log", "message": "x" * 1024}).encode() + b"\n"
        self.path.write_bytes(data)
        requests = []
        returned = []

        class TrackingStream(io.BytesIO):
            def __iter__(self):
                raise AssertionError("unbounded line iteration")

            def readline(self, size=-1):
                requests.append(size)
                line = super().readline(size)
                returned.append(len(line))
                return line

        with mock.patch("qa3_mozlog.MAX_EVENT_BYTES", limit, create=True):
            with mock.patch.object(Path, "open", return_value=TrackingStream(data)):
                with self.assertRaisesRegex(ValueError, "budget"):
                    verify_mozlog(self.path, self.case, self.root, self.process)
        self.assertEqual(requests, [limit + 1])
        self.assertEqual(returned, [limit + 1])

    def test_event_byte_limit_includes_terminator_at_boundary(self):
        limit = 256
        empty = {"action": "log", "level": "INFO", "message": ""}
        base = (json.dumps(empty) + "\n").encode()
        prefix = "".join(json.dumps(event) + "\n" for event in self.events).encode()
        for size in (limit - 1, limit, limit + 1):
            event = {**empty, "message": "x" * (size - len(base))}
            line = (json.dumps(event) + "\n").encode()
            self.assertEqual(len(line), size)
            self.path.write_bytes(prefix + line)
            with self.subTest(size=size), mock.patch(
                "qa3_mozlog.MAX_EVENT_BYTES", limit, create=True
            ):
                if size <= limit:
                    result = verify_mozlog(
                        self.path, self.case, self.root, self.process
                    )
                    self.assertEqual(result["verdict"], "PASS")
                else:
                    with self.assertRaisesRegex(ValueError, "budget"):
                        verify_mozlog(self.path, self.case, self.root, self.process)

    def test_unterminated_complete_and_partial_events_are_rejected(self):
        complete = "".join(json.dumps(event) + "\n" for event in self.events).encode()
        for data in (
            complete[:-1],
            complete + b'{"action":"log","level":"INFO"}',
            complete + b'{"action":"log"',
            complete + b'{"action":"log","level":"INFO"}\r',
        ):
            self.path.write_bytes(data)
            with self.subTest(data=data), self.assertRaises(ValueError):
                verify_mozlog(self.path, self.case, self.root, self.process)

    def test_multibyte_byte_bound_and_truncation_fail_closed(self):
        prefix = "".join(json.dumps(event) + "\n" for event in self.events).encode()
        line = (
            json.dumps(
                {"action": "log", "level": "INFO", "message": "雪" * 80},
                ensure_ascii=False,
            )
            + "\n"
        ).encode()
        self.assertGreater(len(line), len(line.decode()))
        self.path.write_bytes(prefix + line)
        with mock.patch("qa3_mozlog.MAX_EVENT_BYTES", len(line), create=True):
            self.assertEqual(
                verify_mozlog(self.path, self.case, self.root, self.process)["verdict"],
                "PASS",
            )
        for limit in (len(line) - 1, len(line.decode())):
            with self.subTest(limit=limit), mock.patch(
                "qa3_mozlog.MAX_EVENT_BYTES", limit, create=True
            ):
                with self.assertRaisesRegex(ValueError, "budget"):
                    verify_mozlog(self.path, self.case, self.root, self.process)
        self.path.write_bytes(prefix + b'{"action":"log","message":"\xe9\x9b"}\n')
        with self.assertRaises(ValueError):
            verify_mozlog(self.path, self.case, self.root, self.process)

    def test_crlf_and_multiple_complete_events_are_preserved(self):
        data = "".join(json.dumps(event) + "\r\n" for event in self.events).encode()
        self.path.write_bytes(data)
        result = verify_mozlog(self.path, self.case, self.root, self.process)
        self.assertEqual(result["verdict"], "PASS")
        self.assertEqual(result["rawEvents"], len(self.events))

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
