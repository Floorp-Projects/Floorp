#!/usr/bin/env python3
# Adapted from https://github.com/tc39/test262/blob/main/tools/generation/test/run.py

import contextlib
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

import pytest
from mozunit import main

testDir = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.abspath(os.path.join(testDir, "..", "out"))
EXPECTED_DIR = os.path.join(testDir, "expected")
ex = os.path.join(testDir, "..", "test262-export.py")
importExec = os.path.join(testDir, "..", "test262-update.py")
test262Url = "https://github.com/tc39/test262.git"


@contextlib.contextmanager
def TemporaryDirectory():
    tmpDir = tempfile.mkdtemp()
    try:
        yield tmpDir
    finally:
        shutil.rmtree(tmpDir)


class TestExport(unittest.TestCase):
    maxDiff = None

    def exportScript(self):
        abspath = os.path.abspath(os.path.join(testDir, "fixtures", "export"))
        sp = subprocess.Popen(
            [sys.executable, os.path.abspath(ex), abspath, "--out", OUT_DIR],
            stdout=subprocess.PIPE,
            cwd=os.path.join(testDir, ".."),
        )
        stdout, stderr = sp.communicate()
        return dict(stdout=stdout, stderr=stderr, returncode=sp.returncode)

    def importLocal(self):
        with TemporaryDirectory() as cloneDir:
            branch = "smTempBranch"
            # Clone Test262 to a local branch
            subprocess.check_call(["git", "clone", "--depth=1", test262Url, cloneDir])
            # Checkout to a new branch
            subprocess.check_call(["git", "-C", cloneDir, "checkout", "-b", branch])
            # Make changes on the new branch
            # Remove test/language/export/escaped-from.js
            subprocess.check_call(
                ["git", "-C", cloneDir, "rm", "test/language/export/escaped-from.js"]
            )
            # Rename test/language/export/escaped-default.js
            subprocess.check_call(
                [
                    "git",
                    "-C",
                    cloneDir,
                    "mv",
                    "test/language/export/escaped-default.js",
                    "test/language/export/escaped-foobarbaz.js",
                ]
            )
            # Copy fixtures files
            fixturesDir = os.path.join(testDir, "fixtures", "import", "files")
            shutil.copytree(fixturesDir, os.path.join(cloneDir, "test", "temp42"))
            # Stage and Commit changes
            subprocess.check_call(["git", "-C", cloneDir, "add", "."])
            subprocess.check_call(
                ["git", "-C", cloneDir, "commit", "-m", '"local foo"']
            )

            # Run import script
            print(
                "%s %s --local %s --out %s"
                % (sys.executable, importExec, cloneDir, OUT_DIR)
            )
            sp = subprocess.Popen(
                [sys.executable, importExec, "--local", cloneDir, "--out", OUT_DIR],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                cwd=os.path.join(testDir, ".."),
            )
            stdoutdata, stderrdata = sp.communicate()

            return stdoutdata, stderrdata, sp.returncode, cloneDir

    def isTestFile(self, filename: str):
        return not (
            filename.startswith(".")
            or filename.startswith("#")
            or filename.endswith("~")
        )

    def getFiles(self, path: str):
        names: list[str] = []
        for root, _, fileNames in os.walk(path):
            for fileName in filter(self.isTestFile, fileNames):
                names.append(os.path.join(root, fileName))
        names.sort()
        return names

    def compareTrees(self, expectedPath: str, actualPath: str):
        expectedFiles = self.getFiles(expectedPath)
        actualFiles = self.getFiles(actualPath)

        self.assertListEqual(
            [os.path.relpath(x, expectedPath) for x in expectedFiles],
            [os.path.relpath(x, actualPath) for x in actualFiles],
        )

        for expectedFile, actualFile in zip(expectedFiles, actualFiles):
            with open(expectedFile) as expectedHandle:
                with open(actualFile) as actualHandle:
                    self.assertMultiLineEqual(
                        expectedHandle.read(), actualHandle.read(), expectedFile
                    )

    def compareContents(self, output: bytes, filePath: str, folder: str):
        with open(filePath, "r") as file:
            expected = file.read()

        expected = expected.replace("{{folder}}", folder)
        self.assertMultiLineEqual(output.decode("utf-8"), expected)

    def tearDown(self):
        shutil.rmtree(OUT_DIR, ignore_errors=True)

    def test_export(self):
        result = self.exportScript()
        self.assertEqual(result["returncode"], 0)
        expectedPath = os.path.join(EXPECTED_DIR, "export")
        actualPath = os.path.join(OUT_DIR, "tests", "export")
        self.compareTrees(expectedPath, actualPath)

    @pytest.mark.skip(reason="test fetches from github")
    def test_import_local(self):
        stdoutdata, stderrdata, returncode, folder = self.importLocal()
        self.assertEqual(stderrdata, b"")
        self.assertEqual(returncode, 0)
        self.compareTrees(
            os.path.join(EXPECTED_DIR, "import", "files", "local"),
            os.path.join(OUT_DIR, "local"),
        )
        self.compareContents(
            stdoutdata,
            os.path.join(testDir, "expected", "import", "output.txt"),
            folder,
        )


if __name__ == "__main__":
    main()
