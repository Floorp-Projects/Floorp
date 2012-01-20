from __future__ import with_statement
import subprocess
import unittest
import sys
import os
import imp
from tempfile import mkdtemp
from shutil import rmtree
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
from mozunit import MozTestRunner

from UserString import UserString
# Create a controlled configuration for use by expandlibs
config_win = {
    'AR_EXTRACT': '',
    'DLL_PREFIX': '',
    'LIB_PREFIX': '',
    'OBJ_SUFFIX': '.obj',
    'LIB_SUFFIX': '.lib',
    'DLL_SUFFIX': '.dll',
    'IMPORT_LIB_SUFFIX': '.lib',
    'LIBS_DESC_SUFFIX': '.desc',
    'EXPAND_LIBS_LIST_STYLE': 'list',
}
config_unix = {
    'AR_EXTRACT': 'ar -x',
    'DLL_PREFIX': 'lib',
    'LIB_PREFIX': 'lib',
    'OBJ_SUFFIX': '.o',
    'LIB_SUFFIX': '.a',
    'DLL_SUFFIX': '.so',
    'IMPORT_LIB_SUFFIX': '',
    'LIBS_DESC_SUFFIX': '.desc',
    'EXPAND_LIBS_LIST_STYLE': 'linkerscript',
}

config = sys.modules['expandlibs_config'] = imp.new_module('expandlibs_config')

from expandlibs import LibDescriptor, ExpandArgs, relativize
from expandlibs_gen import generate
from expandlibs_exec import ExpandArgsMore

def Lib(name):
    return config.LIB_PREFIX + name + config.LIB_SUFFIX

def Obj(name):
    return name + config.OBJ_SUFFIX

def Dll(name):
    return config.DLL_PREFIX + name + config.DLL_SUFFIX

def ImportLib(name):
    if not len(config.IMPORT_LIB_SUFFIX): return Dll(name)
    return config.LIB_PREFIX + name + config.IMPORT_LIB_SUFFIX

class TestRelativize(unittest.TestCase):
    def test_relativize(self):
        '''Test relativize()'''
        os_path_exists = os.path.exists
        def exists(path):
            return True
        os.path.exists = exists
        self.assertEqual(relativize(os.path.abspath(os.curdir)), os.curdir)
        self.assertEqual(relativize(os.path.abspath(os.pardir)), os.pardir)
        self.assertEqual(relativize(os.path.join(os.curdir, 'a')), 'a')
        self.assertEqual(relativize(os.path.join(os.path.abspath(os.curdir), 'a')), 'a')
        # relativize is expected to return the absolute path if it is shorter
        self.assertEqual(relativize(os.sep), os.sep)
        os.path.exists = os.path.exists

class TestLibDescriptor(unittest.TestCase):
    def test_serialize(self):
        '''Test LibDescriptor's serialization'''
        desc = LibDescriptor()
        desc[LibDescriptor.KEYS[0]] = ['a', 'b']
        self.assertEqual(str(desc), "%s = a b" % LibDescriptor.KEYS[0])
        desc['unsupported-key'] = ['a']
        self.assertEqual(str(desc), "%s = a b" % LibDescriptor.KEYS[0])
        desc[LibDescriptor.KEYS[1]] = ['c', 'd', 'e']
        self.assertEqual(str(desc), "%s = a b\n%s = c d e" % (LibDescriptor.KEYS[0], LibDescriptor.KEYS[1]))
        desc[LibDescriptor.KEYS[0]] = []
        self.assertEqual(str(desc), "%s = c d e" % (LibDescriptor.KEYS[1]))

    def test_read(self):
        '''Test LibDescriptor's initialization'''
        desc_list = ["# Comment",
                     "%s = a b" % LibDescriptor.KEYS[1],
                     "", # Empty line
                     "foo = bar", # Should be discarded
                     "%s = c d e" % LibDescriptor.KEYS[0]]
        desc = LibDescriptor(desc_list)
        self.assertEqual(desc[LibDescriptor.KEYS[1]], ['a', 'b'])
        self.assertEqual(desc[LibDescriptor.KEYS[0]], ['c', 'd', 'e'])
        self.assertEqual(False, 'foo' in desc)

def wrap_method(conf, wrapped_method):
    '''Wrapper used to call a test with a specific configuration'''
    def _method(self):
        for key in conf:
            setattr(config, key, conf[key])
        self.init()
        try:
            wrapped_method(self)
        except:
            raise
        finally:
            self.cleanup()
    return _method

class ReplicateTests(type):
    '''Replicates tests for unix and windows variants'''
    def __new__(cls, clsName, bases, dict):
        for name in [key for key in dict if key.startswith('test_')]:
            dict[name + '_unix'] = wrap_method(config_unix, dict[name])
            dict[name + '_unix'].__doc__ = dict[name].__doc__ + ' (unix)'
            dict[name + '_win'] = wrap_method(config_win, dict[name])
            dict[name + '_win'].__doc__ = dict[name].__doc__ + ' (win)'
            del dict[name]
        return type.__new__(cls, clsName, bases, dict)

class TestCaseWithTmpDir(unittest.TestCase):
    __metaclass__ = ReplicateTests
    def init(self):
        self.tmpdir = os.path.abspath(mkdtemp(dir=os.curdir))

    def cleanup(self):
        rmtree(self.tmpdir)

    def touch(self, files):
        for f in files:
            open(f, 'w').close()

    def tmpfile(self, *args):
        return os.path.join(self.tmpdir, *args)

class TestExpandLibsGen(TestCaseWithTmpDir):
    def test_generate(self):
        '''Test library descriptor generation'''
        files = [self.tmpfile(f) for f in
                 [Lib('a'), Obj('b'), Lib('c'), Obj('d'), Obj('e'), Lib('f')]]
        self.touch(files[:-1])
        self.touch([files[-1] + config.LIBS_DESC_SUFFIX])

        desc = generate(files)
        self.assertEqual(desc['OBJS'], [self.tmpfile(Obj(s)) for s in ['b', 'd', 'e']])
        self.assertEqual(desc['LIBS'], [self.tmpfile(Lib(s)) for s in ['a', 'c', 'f']])

class TestExpandInit(TestCaseWithTmpDir):
    def init(self):
        ''' Initializes test environment for library expansion tests'''
        super(TestExpandInit, self).init()
        # Create 2 fake libraries, each containing 3 objects, and the second
        # including the first one and another library.
        os.mkdir(self.tmpfile('libx'))
        os.mkdir(self.tmpfile('liby'))
        self.libx_files = [self.tmpfile('libx', Obj(f)) for f in ['g', 'h', 'i']]
        self.liby_files = [self.tmpfile('liby', Obj(f)) for f in ['j', 'k', 'l']] + [self.tmpfile('liby', Lib('z'))]
        self.touch(self.libx_files + self.liby_files)
        with open(self.tmpfile('libx', Lib('x') + config.LIBS_DESC_SUFFIX), 'w') as f:
            f.write(str(generate(self.libx_files)))
        with open(self.tmpfile('liby', Lib('y') + config.LIBS_DESC_SUFFIX), 'w') as f:
            f.write(str(generate(self.liby_files + [self.tmpfile('libx', Lib('x'))])))

        # Create various objects and libraries 
        self.arg_files = [self.tmpfile(f) for f in [Lib('a'), Obj('b'), Obj('c'), Lib('d'), Obj('e')]]
        # We always give library names (LIB_PREFIX/SUFFIX), even for
        # dynamic/import libraries
        self.files = self.arg_files + [self.tmpfile(ImportLib('f'))]
        self.arg_files += [self.tmpfile(Lib('f'))]
        self.touch(self.files)

    def assertRelEqual(self, args1, args2):
        self.assertEqual(args1, [relativize(a) for a in args2])

class TestExpandArgs(TestExpandInit):
    def test_expand(self):
        '''Test library expansion'''
        # Expanding arguments means libraries with a descriptor are expanded
        # with the descriptor content, and import libraries are used when
        # a library doesn't exist
        args = ExpandArgs(['foo', '-bar'] + self.arg_files + [self.tmpfile('liby', Lib('y'))])
        self.assertRelEqual(args, ['foo', '-bar'] + self.files + self.liby_files + self.libx_files) 

        # When a library exists at the same time as a descriptor, we just use
        # the library
        self.touch([self.tmpfile('libx', Lib('x'))])
        args = ExpandArgs(['foo', '-bar'] + self.arg_files + [self.tmpfile('liby', Lib('y'))])
        self.assertRelEqual(args, ['foo', '-bar'] + self.files + self.liby_files + [self.tmpfile('libx', Lib('x'))]) 

        self.touch([self.tmpfile('liby', Lib('y'))])
        args = ExpandArgs(['foo', '-bar'] + self.arg_files + [self.tmpfile('liby', Lib('y'))])
        self.assertRelEqual(args, ['foo', '-bar'] + self.files + [self.tmpfile('liby', Lib('y'))])

class TestExpandArgsMore(TestExpandInit):
    def test_makelist(self):
        '''Test grouping object files in lists'''
        # ExpandArgsMore does the same as ExpandArgs
        with ExpandArgsMore(['foo', '-bar'] + self.arg_files + [self.tmpfile('liby', Lib('y'))]) as args:
            self.assertRelEqual(args, ['foo', '-bar'] + self.files + self.liby_files + self.libx_files) 

            # But also has an extra method replacing object files with a list
            args.makelist()
            # self.files has objects at #1, #2, #4
            self.assertRelEqual(args[:3], ['foo', '-bar'] + self.files[:1])
            self.assertRelEqual(args[4:], [self.files[3]] + self.files[5:] + [self.tmpfile('liby', Lib('z'))])

            # Check the list file content
            objs = [f for f in self.files + self.liby_files + self.libx_files if f.endswith(config.OBJ_SUFFIX)]
            if config.EXPAND_LIBS_LIST_STYLE == "linkerscript":
                self.assertNotEqual(args[3][0], '@')
                filename = args[3]
                content = ["INPUT(%s)" % relativize(f) for f in objs]
                with open(filename, 'r') as f:
                    self.assertEqual([l.strip() for l in f.readlines() if len(l.strip())], content)
            elif config.EXPAND_LIBS_LIST_STYLE == "list":
                self.assertEqual(args[3][0], '@')
                filename = args[3][1:]
                content = objs
                with open(filename, 'r') as f:
                    self.assertRelEqual([l.strip() for l in f.readlines() if len(l.strip())], content)

            tmp = args.tmp
        # Check that all temporary files are properly removed
        self.assertEqual(True, all([not os.path.exists(f) for f in tmp]))

    def test_extract(self):
        '''Test library extraction'''
        # Divert subprocess.call
        subprocess_call = subprocess.call
        extracted = {}
        def call(args, **kargs):
            # The command called is always AR_EXTRACT
            ar_extract = config.AR_EXTRACT.split()
            self.assertRelEqual(args[:len(ar_extract)], ar_extract)
            # Remaining argument is always one library
            self.assertRelEqual([os.path.splitext(arg)[1] for arg in args[len(ar_extract):]], [config.LIB_SUFFIX])
            # Simulate AR_EXTRACT extracting one object file for the library
            lib = os.path.splitext(os.path.basename(args[len(ar_extract)]))[0]
            extracted[lib] = os.path.join(kargs['cwd'], "%s" % Obj(lib))
            self.touch([extracted[lib]])
        subprocess.call = call

        # ExpandArgsMore does the same as ExpandArgs
        self.touch([self.tmpfile('liby', Lib('y'))])
        with ExpandArgsMore(['foo', '-bar'] + self.arg_files + [self.tmpfile('liby', Lib('y'))]) as args:
            self.assertRelEqual(args, ['foo', '-bar'] + self.files + [self.tmpfile('liby', Lib('y'))])

            # ExpandArgsMore also has an extra method extracting static libraries
            # when possible
            args.extract()

            files = self.files + self.liby_files + self.libx_files
            if not len(config.AR_EXTRACT):
                # If we don't have an AR_EXTRACT, extract() expands libraries with a
                # descriptor when the corresponding library exists (which ExpandArgs
                # alone doesn't)
                self.assertRelEqual(args, ['foo', '-bar'] + files)
            else:
                # With AR_EXTRACT, it uses the descriptors when there are, and actually
                # extracts the remaining libraries
                self.assertRelEqual(args, ['foo', '-bar'] + [extracted[os.path.splitext(os.path.basename(f))[0]] if f.endswith(config.LIB_SUFFIX) else f for f in files])

            tmp = args.tmp
        # Check that all temporary files are properly removed
        self.assertEqual(True, all([not os.path.exists(f) for f in tmp]))

        # Restore subprocess.call
        subprocess.call = subprocess_call

    def test_reorder(self):
        '''Test object reordering'''
        # We don't care about AR_EXTRACT testing, which is done in test_extract
        config.AR_EXTRACT = ''

        # ExpandArgsMore does the same as ExpandArgs
        with ExpandArgsMore(['foo', '-bar'] + self.arg_files + [self.tmpfile('liby', Lib('y'))]) as args:
            self.assertRelEqual(args, ['foo', '-bar'] + self.files + self.liby_files + self.libx_files) 

            # Use an order containing object files from libraries
            order_files = [self.libx_files[1], self.libx_files[0], self.liby_files[2], self.files[1]]
            order = [os.path.splitext(os.path.basename(f))[0] for f in order_files]
            args.reorder(order[:2] + ['unknown'] + order[2:])

            # self.files has objects at #1, #2, #4
            self.assertRelEqual(args[:3], ['foo', '-bar'] + self.files[:1])
            self.assertRelEqual(args[3:7], order_files)
            self.assertRelEqual(args[7:9], [self.files[2], self.files[4]])
            self.assertRelEqual(args[9:11], self.liby_files[:2])
            self.assertRelEqual(args[11:12], [self.libx_files[2]])
            self.assertRelEqual(args[12:14], [self.files[3], self.files[5]])
            self.assertRelEqual(args[14:], [self.liby_files[3]])

if __name__ == '__main__':
    unittest.main(testRunner=MozTestRunner())
