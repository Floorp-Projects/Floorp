#!/usr/bin/env python
# Adapted from https://github.com/tc39/test262/blob/master/tools/generation/test/run.py

import shutil, subprocess, sys, os, unittest

testDir = os.path.dirname(os.path.relpath(__file__))
OUT_DIR = os.path.join(testDir, 'out')
EXPECTED_DIR = os.path.join(testDir, 'expected')
ex = os.path.join(testDir, '..', 'test262-export.py')

class TestExport(unittest.TestCase):
    maxDiff = None

    def fixture(self, name):
        relpath = os.path.relpath(os.path.join(testDir, 'fixtures', name))
        sp = subprocess.Popen(
            [ex, relpath, '--out', OUT_DIR],
            stdout=subprocess.PIPE)
        stdout, stderr = sp.communicate()
        return dict(stdout=stdout, stderr=stderr, returncode=sp.returncode)

    def isTestFile(self, filename):
        return not (
            filename.startswith('.') or
            filename.startswith('#') or
            filename.endswith('~')
        )

    def getFiles(self, path):
        names = []
        for root, _, fileNames in os.walk(path):
            for fileName in filter(self.isTestFile, fileNames):
                names.append(os.path.join(root, fileName))
        names.sort()
        return names

    def compareTrees(self, targetName):
        expectedPath = os.path.join(EXPECTED_DIR, targetName)
        actualPath = OUT_DIR

        expectedFiles = self.getFiles(expectedPath)
        actualFiles = self.getFiles(actualPath)

        self.assertListEqual(
            map(lambda x: os.path.relpath(x, expectedPath), expectedFiles),
            map(lambda x: os.path.relpath(x, actualPath), actualFiles))

        for expectedFile, actualFile in zip(expectedFiles, actualFiles):
            with open(expectedFile) as expectedHandle:
                with open(actualFile) as actualHandle:
                    self.assertMultiLineEqual(
                        expectedHandle.read(),
                        actualHandle.read())

    def tearDown(self):
        shutil.rmtree(OUT_DIR, ignore_errors=True)

    def test_export(self):
        result = self.fixture('export')
        self.assertEqual(result['returncode'], 0)
        self.compareTrees('export')

if __name__ == '__main__':
    unittest.main()
