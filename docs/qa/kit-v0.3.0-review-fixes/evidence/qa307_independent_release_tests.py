"""Independent, synthetic QA307 boundary checks; no installer/native/network."""
from __future__ import annotations

import copy
import json
import os
from pathlib import Path
import sys
import tempfile
import threading
import time
import unittest
from unittest import mock

KIT = Path("/workspace/qa-review-v030/fixes/kit-v030/Floorp_QA_Implementation_Kit_v0.3.0")
RUNTIME = Path("/workspace/qa-review-v030/repos/Floorp-Runtime/.github/workflows/scripts")
sys.path.insert(0, str(KIT / "tools/qa"))
sys.path.insert(0, str(KIT / "tools/qa/tests"))
sys.path.insert(0, str(RUNTIME))
import core
import agent
import test_core
import test_review_regressions
import test_qa3_mozlog


class IndependentBoundaries(unittest.TestCase):
    def setUp(self):
        self.f = test_core.KitTest()
        self.f.setUp()
        self.addCleanup(self.f.doCleanups)

    def test_result_and_schema_exact_attempt_boundary(self):
        f = self.f
        case = f.plan["cases"][0]
        for attempt in (1, 100):
            row = core.result(f.plan, case["id"], "PASS", case["kind"], f.evidence, {}, attempt)
            self.assertEqual(row["attempt"], attempt)
        for attempt in (0, -1, 101, 2**53 - 1, True, False, 1.0, "1"):
            with self.subTest(attempt=attempt), self.assertRaises(core.QAError):
                core.result(f.plan, case["id"], "PASS", case["kind"], f.evidence, {}, attempt)
        schema = json.loads((KIT / "qa/schemas/result.schema.json").read_text())
        self.assertEqual(schema["properties"]["attempt"]["minimum"], 1)
        self.assertEqual(schema["properties"]["attempt"]["maximum"], 100)
        self.assertEqual(core.MAX_ATTEMPTS, 100)

    def test_complete_aggregate_limit_and_one_extra(self):
        f = self.f
        rows = [dict(copy.deepcopy(row), attempt=attempt)
                for row in f.records for attempt in range(1, 101)]
        verdict = f.evaluate(rows)
        self.assertEqual(verdict["status"], "PASS")
        self.assertIs(verdict["publicationAuthorized"], False)
        with self.assertRaisesRegex(core.QAError, "record limit"):
            f.evaluate(rows + [copy.deepcopy(rows[-1])])

    def test_failure_skip_duplicate_and_hole_still_block(self):
        f = self.f
        for status in ("FAIL", "ERROR", "TIMEOUT", "CRASH", "SKIP", "INCOMPLETE"):
            rows = copy.deepcopy(f.records)
            rows[0]["status"] = status
            rows.append(dict(copy.deepcopy(f.records[0]), attempt=2))
            with self.subTest(status=status):
                self.assertEqual(f.evaluate(rows)["status"], "BLOCK")
        self.assertEqual(f.evaluate(f.records + [copy.deepcopy(f.records[0])])["status"], "BLOCK")
        self.assertEqual(f.evaluate(f.records + [dict(copy.deepcopy(f.records[0]), attempt=3)])["status"], "BLOCK")

    def test_selection_extra_foreign_and_grouped_duplicate_cannot_pass(self):
        for selected in (["other.js"], ["browser_example.js", "other.js"],
                         {"one": ["browser_example.js"], "two": ["browser_example.js"]}):
            values = self.f.happy_events()
            values[0]["tests"] = selected
            try:
                verdict = self.f.log(values)
            except core.QAError:
                continue
            with self.subTest(selected=selected):
                self.assertNotEqual(verdict["status"], "PASS")

    def test_blank_missing_or_non_string_assertion_rejected(self):
        for subtest in (None, "", " \n\t", 0, False):
            values = self.f.happy_events()
            values[2]["subtest"] = subtest
            with self.subTest(subtest=subtest), self.assertRaises(core.QAError):
                self.f.log(values)

    def test_total_bytes_and_event_limits_are_inclusive(self):
        values = self.f.happy_events()
        path = self.f.root / "bounded.jsonl"
        raw = b"".join((json.dumps(v) + "\n").encode() for v in values)
        path.write_bytes(raw)
        with mock.patch("core.MAX_MOZLOG_BYTES", len(raw)), mock.patch("core.MAX_MOZLOG_EVENTS", 5):
            self.assertEqual(core.import_mozlog(path, ["browser_example.js"], self.f.root)["status"], "PASS")
        with mock.patch("core.MAX_MOZLOG_BYTES", len(raw) - 1), self.assertRaises(core.QAError):
            core.import_mozlog(path, ["browser_example.js"], self.f.root)
        with mock.patch("core.MAX_MOZLOG_EVENTS", 4), self.assertRaises(core.QAError):
            core.import_mozlog(path, ["browser_example.js"], self.f.root)
        self.assertEqual(core.MAX_MOZLOG_BYTES, 128 * 1024**2)
        self.assertEqual(core.MAX_MOZLOG_EVENTS, 1_000_000)

    def test_real_four_mib_line_boundary(self):
        cap = 4 * 1024**2
        self.assertEqual(core.MAX_MOZLOG_LINE_BYTES, cap)
        base = json.dumps({"action": "process_output", "data": ""}) + "\n"
        for size in (cap, cap + 1):
            diagnostic = (json.dumps({"action": "process_output", "data": "x" * (size - len(base))}) + "\n").encode()
            self.assertEqual(len(diagnostic), size)
            path = self.f.root / ("line-" + str(size) + ".jsonl")
            path.write_bytes(diagnostic + b"".join((json.dumps(v) + "\n").encode() for v in self.f.happy_events()))
            if size == cap:
                self.assertEqual(core.import_mozlog(path, ["browser_example.js"], self.f.root)["status"], "PASS")
            else:
                with self.assertRaisesRegex(core.QAError, "Oversize"):
                    core.import_mozlog(path, ["browser_example.js"], self.f.root)

    def test_65536_byte_agent_frame_is_inclusive(self):
        for size in (65536, 65537):
            path = self.f.root / ("agent-" + str(size))
            path.write_bytes(b"x" * (size - 1) + b"\n")
            with path.open("rb") as stream:
                reader = agent.ActionReader(stream)
                try:
                    if size == 65536:
                        self.assertEqual(len(reader.read(time.monotonic() + 1)), size)
                    else:
                        with self.assertRaises(core.QAError):
                            reader.read(time.monotonic() + 1)
                finally:
                    reader.close()
                self.assertTrue(os.get_blocking(stream.fileno()))

    def test_slow_partial_input_keeps_single_deadline(self):
        read_fd, write_fd = os.pipe()
        stream = os.fdopen(read_fd, "rb", buffering=0)
        stopped = threading.Event()
        def write_slowly():
            try:
                while not stopped.wait(0.015):
                    os.write(write_fd, b"x")
            finally:
                os.close(write_fd)
        writer = threading.Thread(target=write_slowly)
        writer.start()
        reader = agent.ActionReader(stream)
        start = time.monotonic()
        try:
            self.assertIsNone(reader.read(start + 0.15))
            self.assertLess(time.monotonic() - start, 1)
            self.assertGreater(len(reader.buffer), 0)
        finally:
            stopped.set()
            writer.join(timeout=1)
            reader.close()
            self.assertTrue(os.get_blocking(stream.fileno()))
            stream.close()
        self.assertFalse(writer.is_alive())


if __name__ == "__main__":
    loader = unittest.TestLoader()
    suite = unittest.TestSuite([
        loader.loadTestsFromTestCase(IndependentBoundaries),
        loader.loadTestsFromTestCase(test_review_regressions.ReviewRegressions),
        loader.loadTestsFromTestCase(test_qa3_mozlog.MozlogContracts),
    ])
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    print(json.dumps({"testsRun": result.testsRun, "failures": len(result.failures),
                      "errors": len(result.errors), "skips": len(result.skipped),
                      "successful": result.wasSuccessful()}))
    sys.exit(0 if result.wasSuccessful() and not result.skipped else 1)
