import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import cocoa_probe as probe


class ProbeGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = Path(os.environ["COCOA_PROBE_SOURCE_DIR"])

    def test_exact_source_patch_and_fragments(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory) / "generated"
            proof = probe.prepare(self.source, work)
            self.assertEqual(proof["patchedServiceSHA256"], probe.PATCHED_SHA)
            text = (work / "Probe.mm").read_text()
            method, payload = probe.fragments((work / probe.PREFIX / "MacWebAppService.mm").read_text())
            self.assertIn(method, text)
            self.assertIn(payload, text)
            self.assertNotIn("@@", text)
            self.assertEqual(text.count("Check("), 26)
            self.assertEqual((work / probe.PREFIX / "Protocol.mm").read_bytes(),
                             (self.source / probe.PREFIX / "Protocol.mm").read_bytes())

    def test_unpatched_source_cannot_produce_a_probe(self):
        with self.assertRaisesRegex(ValueError, "fragment-count"):
            probe.fragments((self.source / probe.PREFIX / "MacWebAppService.mm").read_text())

    def test_duplicated_fragments_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory) / "generated"
            probe.prepare(self.source, work)
            text = (work / probe.PREFIX / "MacWebAppService.mm").read_text()
            method, _ = probe.fragments(text)
            with self.assertRaisesRegex(ValueError, "fragment-count"):
                probe.fragments(text + "\n" + method)

    def test_wrong_source_digest_is_rejected_before_writes(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory) / "generated"
            with patch.dict(probe.INPUTS, {"MacWebAppService.mm":"0" * 64}):
                with self.assertRaisesRegex(ValueError, "source-digest"):
                    probe.prepare(self.source, work)
            self.assertFalse(work.exists())

    def test_wrong_patch_digest_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(probe, "PATCH_SHA", "0" * 64):
            with self.assertRaisesRegex(ValueError, "patch-digest"):
                probe.prepare(self.source, Path(directory) / "generated")

    def test_preexisting_work_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "work-already-exists"):
                probe.prepare(self.source, Path(directory))

    def test_non_native_platform_fails_without_fetch(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(probe.sys, "platform", "linux"), patch.object(probe, "fetch_source") as fetch:
            path = Path(directory)
            self.assertEqual(probe.run(path / "work", path / "result.json"), 1)
            fetch.assert_not_called()
            report = json.loads((path / "result.json").read_text())
            self.assertEqual(report["status"], "failed")
            self.assertFalse(report["completeHostTranslationUnitCompiled"])
            self.assertFalse(report["originalFailureCauseResolved"])

    def test_fetch_failure_retains_safe_failed_result(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(probe.sys, "platform", "darwin"), patch.object(probe.platform, "machine", return_value="x86_64"), patch.object(probe, "fetch_source", side_effect=OSError("PRIVATE_URL?sig=PRIVATE")):
            path = Path(directory)
            self.assertEqual(probe.run(path / "work", path / "result.json"), 1)
            text = (path / "result.json").read_text()
            self.assertNotIn("PRIVATE", text)
            self.assertEqual(json.loads(text)["error"]["stage"], "fetch-source")

    def test_command_output_is_bounded(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "command-output-limit"):
                probe.capture([sys.executable, "-c", "print('x'*65536)"], Path(directory), 10)


if __name__ == "__main__":
    unittest.main(verbosity=2)
