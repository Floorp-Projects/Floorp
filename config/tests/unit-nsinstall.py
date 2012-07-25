import unittest

import os, sys, os.path, time
from tempfile import mkdtemp
from shutil import rmtree
import mozunit
from mozprocess import processhandler

from nsinstall import nsinstall
import nsinstall as nsinstall_module
NSINSTALL_PATH = nsinstall_module.__file__

# Run the non-ASCII tests on (a) Windows, or (b) any platform with
# sys.stdin.encoding set to UTF-8
import codecs
RUN_NON_ASCII_TESTS = (sys.platform == "win32" or
                       (sys.stdin.encoding is not None and
                        codecs.lookup(sys.stdin.encoding) == codecs.lookup("utf-8")))

class TestNsinstall(unittest.TestCase):
    """
    Unit tests for nsinstall.py
    """
    def setUp(self):
        self.tmpdir = mkdtemp()

    def tearDown(self):
        # Unicode strings means non-ASCII children can be deleted properly on
        # Windows
        if sys.stdin.encoding is None:
            tmpdir = unicode(self.tmpdir)
        else:
            tmpdir = unicode(self.tmpdir, sys.stdin.encoding)
        rmtree(tmpdir)

    # utility methods for tests
    def touch(self, file, dir=None):
        if dir is None:
            dir = self.tmpdir
        f = os.path.join(dir, file)
        open(f, 'w').close()
        return f

    def mkdirs(self, dir):
        d = os.path.join(self.tmpdir, dir)
        os.makedirs(d)
        return d

    def test_nsinstall_D(self):
        "Test nsinstall -D <dir>"
        testdir = os.path.join(self.tmpdir, "test")
        self.assertEqual(nsinstall(["-D", testdir]), 0)
        self.assert_(os.path.isdir(testdir))

    def test_nsinstall_basic(self):
        "Test nsinstall <file> <dir>"
        testfile = self.touch("testfile")
        testdir = self.mkdirs("testdir")
        self.assertEqual(nsinstall([testfile, testdir]), 0)
        self.assert_(os.path.isfile(os.path.join(testdir, "testfile")))

    def test_nsinstall_basic_recursive(self):
        "Test nsinstall <dir> <dest dir>"
        sourcedir = self.mkdirs("sourcedir")
        self.touch("testfile", sourcedir)
        Xfile = self.touch("Xfile", sourcedir)
        copieddir = self.mkdirs("sourcedir/copieddir")
        self.touch("testfile2", copieddir)
        Xdir = self.mkdirs("sourcedir/Xdir")
        self.touch("testfile3", Xdir)

        destdir = self.mkdirs("destdir")

        self.assertEqual(nsinstall([sourcedir, destdir,
                                    '-X', Xfile,
                                    '-X', Xdir]), 0)

        testdir = os.path.join(destdir, "sourcedir")
        self.assert_(os.path.isdir(testdir))
        self.assert_(os.path.isfile(os.path.join(testdir, "testfile")))
        self.assert_(not os.path.exists(os.path.join(testdir, "Xfile")))
        self.assert_(os.path.isdir(os.path.join(testdir, "copieddir")))
        self.assert_(os.path.isfile(os.path.join(testdir, "copieddir", "testfile2")))
        self.assert_(not os.path.exists(os.path.join(testdir, "Xdir")))

    def test_nsinstall_multiple(self):
        "Test nsinstall <three files> <dest dir>"
        testfiles = [self.touch("testfile1"),
                     self.touch("testfile2"),
                     self.touch("testfile3")]
        testdir = self.mkdirs("testdir")
        self.assertEqual(nsinstall(testfiles + [testdir]), 0)
        for f in testfiles:
            self.assert_(os.path.isfile(os.path.join(testdir,
                                                     os.path.basename(f))))

    def test_nsinstall_dir_exists(self):
        "Test nsinstall <dir> <dest dir>, where <dest dir>/<dir> already exists"
        srcdir = self.mkdirs("test")
        destdir = self.mkdirs("testdir/test")
        self.assertEqual(nsinstall([srcdir, os.path.dirname(destdir)]), 0)
        self.assert_(os.path.isdir(destdir))

    def test_nsinstall_t(self):
        "Test that nsinstall -t works (preserve timestamp)"
        testfile = self.touch("testfile")
        testdir = self.mkdirs("testdir")
        # set mtime to now - 30 seconds
        t = int(time.time()) - 30
        os.utime(testfile, (t, t))
        self.assertEqual(nsinstall(["-t", testfile, testdir]), 0)
        destfile = os.path.join(testdir, "testfile")
        self.assert_(os.path.isfile(destfile))
        self.assertEqual(os.stat(testfile).st_mtime,
                         os.stat(destfile).st_mtime)

    if sys.platform != "win32":
        # can't run this test on windows, don't have real file modes there
        def test_nsinstall_m(self):
            "Test that nsinstall -m works (set mode)"
            testfile = self.touch("testfile")
            mode = 0600
            os.chmod(testfile, mode)
            testdir = self.mkdirs("testdir")
            self.assertEqual(nsinstall(["-m", "%04o" % mode, testfile, testdir]), 0)
            destfile = os.path.join(testdir, "testfile")
            self.assert_(os.path.isfile(destfile))
            self.assertEqual(os.stat(testfile).st_mode,
                             os.stat(destfile).st_mode)

    def test_nsinstall_d(self):
        "Test that nsinstall -d works (create directories in target)"
        # -d makes no sense to me, but ok!
        testfile = self.touch("testfile")
        testdir = self.mkdirs("testdir")
        destdir = os.path.join(testdir, "subdir")
        self.assertEqual(nsinstall(["-d", testfile, destdir]), 0)
        self.assert_(os.path.isdir(os.path.join(destdir, "testfile")))

    if RUN_NON_ASCII_TESTS:
        def test_nsinstall_non_ascii(self):
            "Test that nsinstall handles non-ASCII files"
            filename = u"\u2325\u3452\u2415\u5081"
            testfile = self.touch(filename)
            testdir = self.mkdirs(u"\u4241\u1D04\u1414")
            self.assertEqual(nsinstall([testfile.encode("utf-8"),
                                        testdir.encode("utf-8")]), 0)

            destfile = os.path.join(testdir, filename)
            self.assert_(os.path.isfile(destfile))

        def test_nsinstall_non_ascii_subprocess(self):
            "Test that nsinstall as a subprocess handles non-ASCII files"
            filename = u"\u2325\u3452\u2415\u5081"
            testfile = self.touch(filename)
            testdir = self.mkdirs(u"\u4241\u1D04\u1414")
            # We don't use subprocess because it can't handle Unicode on
            # Windows <http://bugs.python.org/issue1759845>. mozprocess calls
            # CreateProcessW directly so it's perfect.
            p = processhandler.ProcessHandlerMixin([sys.executable,
                                                    NSINSTALL_PATH,
                                                    testfile, testdir])
            p.run()
            rv = p.waitForFinish()

            self.assertEqual(rv, 0)
            destfile = os.path.join(testdir, filename)
            self.assert_(os.path.isfile(destfile))

    #TODO: implement -R, -l, -L and test them!

if __name__ == '__main__':
  mozunit.main()
