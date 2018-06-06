#!/usr/bin/env python
# Adapted from https://github.com/tc39/test262/blob/master/tools/generation/test/run.py

import shutil
import subprocess
import contextlib
import tempfile
import os
import unittest

testDir = os.path.dirname(os.path.relpath(__file__))
OUT_DIR = os.path.join(testDir, 'out')
EXPECTED_DIR = os.path.join(testDir, 'expected')
ex = os.path.join(testDir, '..', 'test262-export.py')
importExec = os.path.join(testDir, '..', 'test262-update.py')
test262Url = 'git://github.com/tc39/test262.git'


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
        relpath = os.path.relpath(os.path.join(testDir, 'fixtures', 'export'))
        sp = subprocess.Popen(
            [ex, relpath, '--out', OUT_DIR],
            stdout=subprocess.PIPE)
        stdout, stderr = sp.communicate()
        return dict(stdout=stdout, stderr=stderr, returncode=sp.returncode)

    def importLocal(self):
        with TemporaryDirectory() as cloneDir:
            branch = 'smTempBranch'
            # Clone Test262 to a local branch
            subprocess.check_call(
                ['git', 'clone', '--depth=1', test262Url, cloneDir]
            )
            # Checkout to a new branch
            subprocess.check_call(
                ['git', '-C', cloneDir, 'checkout', '-b', branch]
            )
            # Make changes on the new branch
            # Remove test/language/export/escaped-from.js
            subprocess.check_call(
                ['git', '-C', cloneDir, 'rm',
                    'test/language/export/escaped-from.js']
            )
            # Rename test/language/export/escaped-default.js
            subprocess.check_call(
                ['git', '-C', cloneDir, 'mv',
                    'test/language/export/escaped-default.js',
                    'test/language/export/escaped-foobarbaz.js',
                 ]
            )
            # Copy fixtures files
            fixturesDir = os.path.join(testDir, 'fixtures', 'import', 'files')
            shutil.copytree(fixturesDir, os.path.join(cloneDir, 'test', 'temp42'))
            # Stage and Commit changes
            subprocess.check_call(['git', '-C', cloneDir, 'add', '.'])
            subprocess.check_call(['git', '-C', cloneDir, 'commit', '-m', '"local foo"'])

            # Run import script
            print("%s --local %s --out %s" % (importExec, cloneDir, OUT_DIR))
            sp = subprocess.Popen(
                [importExec, '--local', cloneDir, '--out', OUT_DIR],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )
            stdoutdata, _ = sp.communicate()

            return stdoutdata, sp.returncode, cloneDir

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

    def compareContents(self, output, filePath, folder):
        with open(filePath, "rb") as file:
            expected = file.read()

        expected = expected.replace('{{folder}}', folder)
        self.assertMultiLineEqual(output, expected)

    def tearDown(self):
        shutil.rmtree(OUT_DIR, ignore_errors=True)

    def test_export(self):
        result = self.exportScript()
        self.assertEqual(result['returncode'], 0)
        self.compareTrees('export')

    def test_import_local(self):
        output, returncode, folder = self.importLocal()
        self.assertEqual(returncode, 0)
        self.compareTrees(os.path.join('import', 'files'))
        self.compareContents(
            output,
            os.path.join(testDir, 'expected', 'import', 'output.txt'),
            folder
        )


if __name__ == '__main__':
    unittest.main()
