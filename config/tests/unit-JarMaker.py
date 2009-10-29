import unittest

import os, sys, os.path, time, inspect
from filecmp import dircmp
from tempfile import mkdtemp
from shutil import rmtree, copy2
from zipfile import ZipFile
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

from mozunit import MozTestRunner
from JarMaker import JarMaker


class _TreeDiff(dircmp):
    """Helper to report rich results on difference between two directories.
    """
    def _fillDiff(self, dc, rv, basepath="%s"):
        rv['right_only'] += map(lambda l: basepath % l, dc.right_only)
        rv['left_only'] += map(lambda l: basepath % l, dc.left_only)
        rv['diff_files'] += map(lambda l: basepath % l, dc.diff_files)
        rv['funny'] += map(lambda l: basepath % l, dc.common_funny)
        rv['funny'] += map(lambda l: basepath % l, dc.funny_files)
        for subdir, _dc in dc.subdirs.iteritems():
            self._fillDiff(_dc, rv, basepath % (subdir + "/%s"))
    def allResults(self, left, right):
        rv = {'right_only':[], 'left_only':[],
              'diff_files':[], 'funny': []}
        self._fillDiff(self, rv)
        chunks = []
        if rv['right_only']:
            chunks.append('%s only in %s' % (', '.join(rv['right_only']),
                                            right))
        if rv['left_only']:
            chunks.append('%s only in %s' % (', '.join(rv['left_only']),
                                            left))
        if rv['diff_files']:
            chunks.append('%s differ' % ', '.join(rv['diff_files']))
        if rv['funny']:
            chunks.append("%s don't compare" % ', '.join(rv['funny']))
        return '; '.join(chunks)

class TestJarMaker(unittest.TestCase):
    """
    Unit tests for JarMaker.py
    """
    debug = False # set to True to debug failing tests on disk
    def setUp(self):
        self.tmpdir = mkdtemp()
        self.srcdir = os.path.join(self.tmpdir, 'src')
        os.mkdir(self.srcdir)
        self.builddir = os.path.join(self.tmpdir, 'build')
        os.mkdir(self.builddir)
        self.refdir = os.path.join(self.tmpdir, 'ref')
        os.mkdir(self.refdir)
        self.stagedir = os.path.join(self.tmpdir, 'stage')
        os.mkdir(self.stagedir)

    def tearDown(self):
        if self.debug:
            print self.tmpdir
        else:
            rmtree(self.tmpdir)

    def _jar_and_compare(self, *args, **kwargs):
        jm = JarMaker(outputFormat='jar')
        kwargs['jardir'] = os.path.join(self.builddir, 'chrome')
        if 'topsourcedir' not in kwargs:
            kwargs['topsourcedir'] = self.srcdir
        jm.makeJars(*args, **kwargs)
        cwd = os.getcwd()
        os.chdir(self.builddir)
        try:
            # expand build to stage
            for path, dirs, files in os.walk('.'):
                stagedir = os.path.join(self.stagedir, path)
                if not os.path.isdir(stagedir):
                    os.mkdir(stagedir)
                for file in files:
                    if file.endswith('.jar'):
                        # expand jar
                        stagepath = os.path.join(stagedir, file)
                        os.mkdir(stagepath)
                        zf = ZipFile(os.path.join(path, file))
                        # extractall is only in 2.6, do this manually :-(
                        for entry_name in zf.namelist():
                            segs = entry_name.split('/')
                            fname = segs.pop()
                            dname = os.path.join(stagepath, *segs)
                            if not os.path.isdir(dname):
                                os.makedirs(dname)
                            if not fname:
                                # directory, we're done
                                continue
                            _c = zf.read(entry_name)
                            open(os.path.join(dname, fname), 'wb').write(_c)
                        zf.close()
                    else:
                        copy2(os.path.join(path, file), stagedir)
            # compare both dirs
            os.chdir('..')
            td = _TreeDiff('ref', 'stage')
            return td.allResults('reference', 'build')
        finally:
            os.chdir(cwd)

    def test_a_simple_jar(self):
        '''Test a simple jar.mn'''
        # create src content
        jarf = open(os.path.join(self.srcdir, 'jar.mn'), 'w')
        jarf.write('''test.jar:
 dir/foo (bar)
''')
        jarf.close()
        open(os.path.join(self.srcdir,'bar'),'w').write('content\n')
        # create reference
        refpath  = os.path.join(self.refdir, 'chrome', 'test.jar', 'dir')
        os.makedirs(refpath)
        open(os.path.join(refpath, 'foo'), 'w').write('content\n')
        # call JarMaker
        rv = self._jar_and_compare((os.path.join(self.srcdir,'jar.mn'),),
                                   tuple(),
                                   sourcedirs = [self.srcdir])
        self.assertTrue(not rv, rv)

    def test_k_multi_relative_jar(self):
        '''Test the API for multiple l10n jars, with different relative paths'''
        # create app src content
        def _mangle(relpath):
            'method we use to map relpath to srcpaths'
            return os.path.join(self.srcdir, 'other-' + relpath)
        jars = []
        for relpath in ('foo', 'bar'):
            ldir = os.path.join(self.srcdir, relpath, 'locales')
            os.makedirs(ldir)
            jp = os.path.join(ldir, 'jar.mn')
            jars.append(jp)
            open(jp, 'w').write('''ab-CD.jar:
% locale app ab-CD %app
  app/''' + relpath + ' (%' + relpath + ''')
''')
            ldir = _mangle(relpath)
            os.mkdir(ldir)
            open(os.path.join(ldir, relpath), 'w').write(relpath+" content\n")
        # create reference
        chrome_ref = os.path.join(self.refdir, 'chrome')
        os.mkdir(chrome_ref)
        mf = open(os.path.join(chrome_ref, 'ab-CD.manifest'), 'wb')
        mf.write('locale app ab-CD jar:ab-CD.jar!/app\n')
        mf.close()
        ldir = os.path.join(chrome_ref, 'ab-CD.jar', 'app')
        os.makedirs(ldir)
        for relpath in ('foo', 'bar'):
            open(os.path.join(ldir, relpath), 'w').write(relpath+" content\n")
        # call JarMaker
        difference = self._jar_and_compare(jars,
                                           (_mangle,),
                                           sourcedirs = [])
        self.assertTrue(not difference, difference)

if __name__ == '__main__':
    unittest.main(testRunner=MozTestRunner())
