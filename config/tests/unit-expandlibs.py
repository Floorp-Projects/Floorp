import subprocess
import unittest
import sys
import os
import imp
from tempfile import mkdtemp
from shutil import rmtree
import mozunit

from UserString import UserString
# Create a controlled configuration for use by expandlibs
config_win = {
    'AR': 'lib',
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
    'AR': 'ar',
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

from expandlibs import LibDescriptor, ExpandArgs, relativize, ExpandLibsDeps
from expandlibs_gen import generate
from expandlibs_exec import ExpandArgsMore, SectionFinder

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
        self.assertEqual(str(desc), "{0} = a b".format(LibDescriptor.KEYS[0]))
        desc['unsupported-key'] = ['a']
        self.assertEqual(str(desc), "{0} = a b".format(LibDescriptor.KEYS[0]))
        desc[LibDescriptor.KEYS[1]] = ['c', 'd', 'e']
        self.assertEqual(str(desc),
                         "{0} = a b\n{1} = c d e"
                         .format(LibDescriptor.KEYS[0], LibDescriptor.KEYS[1]))
        desc[LibDescriptor.KEYS[0]] = []
        self.assertEqual(str(desc), "{0} = c d e".format(LibDescriptor.KEYS[1]))

    def test_read(self):
        '''Test LibDescriptor's initialization'''
        desc_list = ["# Comment",
                     "{0} = a b".format(LibDescriptor.KEYS[1]),
                     "", # Empty line
                     "foo = bar", # Should be discarded
                     "{0} = c d e".format(LibDescriptor.KEYS[0])]
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

        self.assertRaises(Exception, generate, files + [self.tmpfile(Obj('z'))])
        self.assertRaises(Exception, generate, files + [self.tmpfile(Lib('y'))])

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

class TestExpandLibsDeps(TestExpandInit):
    def test_expandlibsdeps(self):
        '''Test library expansion for dependencies'''
        # Dependency list for a library with a descriptor is equivalent to
        # the arguments expansion, to which we add each descriptor
        args = self.arg_files + [self.tmpfile('liby', Lib('y'))]
        self.assertRelEqual(ExpandLibsDeps(args), ExpandArgs(args) + [self.tmpfile('libx', Lib('x') + config.LIBS_DESC_SUFFIX), self.tmpfile('liby', Lib('y') + config.LIBS_DESC_SUFFIX)])

        # When a library exists at the same time as a descriptor, the
        # descriptor is not a dependency
        self.touch([self.tmpfile('libx', Lib('x'))])
        args = self.arg_files + [self.tmpfile('liby', Lib('y'))]
        self.assertRelEqual(ExpandLibsDeps(args), ExpandArgs(args) + [self.tmpfile('liby', Lib('y') + config.LIBS_DESC_SUFFIX)])

        self.touch([self.tmpfile('liby', Lib('y'))])
        args = self.arg_files + [self.tmpfile('liby', Lib('y'))]
        self.assertRelEqual(ExpandLibsDeps(args), ExpandArgs(args))

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
                content = ['INPUT("{0}")'.format(relativize(f)) for f in objs]
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
            if config.AR == 'lib':
                self.assertEqual(args[:2], [config.AR, '-NOLOGO'])
                self.assertTrue(args[2].startswith('-EXTRACT:'))
                extract = [args[2][len('-EXTRACT:'):]]
                self.assertTrue(extract)
                args = args[3:]
            else:
                # The command called is always AR_EXTRACT
                ar_extract = config.AR_EXTRACT.split()
                self.assertEqual(args[:len(ar_extract)], ar_extract)
                args = args[len(ar_extract):]
            # Remaining argument is always one library
            self.assertEqual(len(args), 1)
            arg = args[0]
            self.assertEqual(os.path.splitext(arg)[1], config.LIB_SUFFIX)
            # Simulate file extraction
            lib = os.path.splitext(os.path.basename(arg))[0]
            if config.AR != 'lib':
                extract = [lib, lib + '2']
            extract = [os.path.join(kargs['cwd'], f) for f in extract]
            if config.AR != 'lib':
                extract = [Obj(f) for f in extract]
            if not lib in extracted:
                extracted[lib] = []
            extracted[lib].extend(extract)
            self.touch(extract)
        subprocess.call = call

        def check_output(args, **kargs):
            # The command called is always AR
            ar = config.AR
            self.assertEqual(args[0:3], [ar, '-NOLOGO', '-LIST'])
            # Remaining argument is always one library
            self.assertRelEqual([os.path.splitext(arg)[1] for arg in args[3:]],
[config.LIB_SUFFIX])
            # Simulate LIB -NOLOGO -LIST
            lib = os.path.splitext(os.path.basename(args[3]))[0]
            return '%s\n%s\n' % (Obj(lib), Obj(lib + '2'))
        subprocess.check_output = check_output

        # ExpandArgsMore does the same as ExpandArgs
        self.touch([self.tmpfile('liby', Lib('y'))])
        with ExpandArgsMore(['foo', '-bar'] + self.arg_files + [self.tmpfile('liby', Lib('y'))]) as args:
            self.assertRelEqual(args, ['foo', '-bar'] + self.files + [self.tmpfile('liby', Lib('y'))])

            # ExpandArgsMore also has an extra method extracting static libraries
            # when possible
            args.extract()

            files = self.files + self.liby_files + self.libx_files
            # With AR_EXTRACT, it uses the descriptors when there are, and
            # actually
            # extracts the remaining libraries
            extracted_args = []
            for f in files:
                if f.endswith(config.LIB_SUFFIX):
                    extracted_args.extend(sorted(extracted[os.path.splitext(os.path.basename(f))[0]]))
                else:
                    extracted_args.append(f)
            self.assertRelEqual(args, ['foo', '-bar'] + extracted_args)

            tmp = args.tmp
        # Check that all temporary files are properly removed
        self.assertEqual(True, all([not os.path.exists(f) for f in tmp]))

        # Restore subprocess.call
        subprocess.call = subprocess_call

class FakeProcess(object):
    def __init__(self, out, err = ''):
        self.out = out
        self.err = err

    def communicate(self):
        return (self.out, self.err)

OBJDUMPS = {
'foo.o': '''
00000000 g     F .text\t00000001 foo
00000000 g     F .text._Z6foobarv\t00000001 _Z6foobarv
00000000 g     F .text.hello\t00000001 hello
00000000 g     F .text._ZThn4_6foobarv\t00000001 _ZThn4_6foobarv
''',
'bar.o': '''
00000000 g     F .text.hi\t00000001 hi
00000000 g     F .text.hot._Z6barbazv\t00000001 .hidden _Z6barbazv
''',
}

PRINT_ICF = '''
ld: ICF folding section '.text.hello' in file 'foo.o'into '.text.hi' in file 'bar.o'
ld: ICF folding section '.foo' in file 'foo.o'into '.foo' in file 'bar.o'
'''

class SubprocessPopen(object):
    def __init__(self, test):
        self.test = test

    def __call__(self, args, stdout = None, stderr = None):
        self.test.assertEqual(stdout, subprocess.PIPE)
        self.test.assertEqual(stderr, subprocess.PIPE)
        if args[0] == 'objdump':
            self.test.assertEqual(args[1], '-t')
            self.test.assertTrue(args[2] in OBJDUMPS)
            return FakeProcess(OBJDUMPS[args[2]])
        else:
            return FakeProcess('', PRINT_ICF)

class TestSectionFinder(unittest.TestCase):
    def test_getSections(self):
        '''Test SectionFinder'''
        # Divert subprocess.Popen
        subprocess_popen = subprocess.Popen
        subprocess.Popen = SubprocessPopen(self)
        config.EXPAND_LIBS_ORDER_STYLE = 'linkerscript'
        config.OBJ_SUFFIX = '.o'
        config.LIB_SUFFIX = '.a'
        finder = SectionFinder(['foo.o', 'bar.o'])
        self.assertEqual(finder.getSections('foobar'), [])
        self.assertEqual(finder.getSections('_Z6barbazv'), ['.text.hot._Z6barbazv'])
        self.assertEqual(finder.getSections('_Z6foobarv'), ['.text._Z6foobarv', '.text._ZThn4_6foobarv'])
        self.assertEqual(finder.getSections('_ZThn4_6foobarv'), ['.text._Z6foobarv', '.text._ZThn4_6foobarv'])
        subprocess.Popen = subprocess_popen

class TestSymbolOrder(unittest.TestCase):
    def test_getOrderedSections(self):
        '''Test ExpandMoreArgs' _getOrderedSections'''
        # Divert subprocess.Popen
        subprocess_popen = subprocess.Popen
        subprocess.Popen = SubprocessPopen(self)
        config.EXPAND_LIBS_ORDER_STYLE = 'linkerscript'
        config.OBJ_SUFFIX = '.o'
        config.LIB_SUFFIX = '.a'
        config.LD_PRINT_ICF_SECTIONS = ''
        args = ExpandArgsMore(['foo', '-bar', 'bar.o', 'foo.o'])
        self.assertEqual(args._getOrderedSections(['_Z6foobarv', '_Z6barbazv']), ['.text._Z6foobarv', '.text._ZThn4_6foobarv', '.text.hot._Z6barbazv'])
        self.assertEqual(args._getOrderedSections(['_ZThn4_6foobarv', '_Z6barbazv']), ['.text._Z6foobarv', '.text._ZThn4_6foobarv', '.text.hot._Z6barbazv'])
        subprocess.Popen = subprocess_popen

    def test_getFoldedSections(self):
        '''Test ExpandMoreArgs' _getFoldedSections'''
        # Divert subprocess.Popen
        subprocess_popen = subprocess.Popen
        subprocess.Popen = SubprocessPopen(self)
        config.LD_PRINT_ICF_SECTIONS = '-Wl,--print-icf-sections'
        args = ExpandArgsMore(['foo', '-bar', 'bar.o', 'foo.o'])
        self.assertEqual(args._getFoldedSections(), {'.text.hello': ['.text.hi'], '.text.hi': ['.text.hello']})
        subprocess.Popen = subprocess_popen

    def test_getOrderedSectionsWithICF(self):
        '''Test ExpandMoreArgs' _getOrderedSections, with ICF'''
        # Divert subprocess.Popen
        subprocess_popen = subprocess.Popen
        subprocess.Popen = SubprocessPopen(self)
        config.EXPAND_LIBS_ORDER_STYLE = 'linkerscript'
        config.OBJ_SUFFIX = '.o'
        config.LIB_SUFFIX = '.a'
        config.LD_PRINT_ICF_SECTIONS = '-Wl,--print-icf-sections'
        args = ExpandArgsMore(['foo', '-bar', 'bar.o', 'foo.o'])
        self.assertEqual(args._getOrderedSections(['hello', '_Z6barbazv']), ['.text.hello', '.text.hi', '.text.hot._Z6barbazv'])
        self.assertEqual(args._getOrderedSections(['_ZThn4_6foobarv', 'hi', '_Z6barbazv']), ['.text._Z6foobarv', '.text._ZThn4_6foobarv', '.text.hi', '.text.hello', '.text.hot._Z6barbazv'])
        subprocess.Popen = subprocess_popen


if __name__ == '__main__':
    mozunit.main()
