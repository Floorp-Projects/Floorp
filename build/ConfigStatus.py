# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Combined with build/autoconf/config.status.m4, ConfigStatus is an almost
# drop-in replacement for autoconf 2.13's config.status, with features
# borrowed from autoconf > 2.5, and additional features.

from __future__ import with_statement
from optparse import OptionParser
import sys, re, os, posixpath, ntpath
import errno
from StringIO import StringIO
from os.path import relpath

# Standalone js doesn't have virtualenv.
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'config'))
from Preprocessor import Preprocessor

# Basic logging facility
verbose = False
def log(string):
    if verbose:
        print >>sys.stderr, string


def ensureParentDir(file):
    '''Ensures the directory parent to the given file exists'''
    dir = os.path.dirname(file)
    if dir and not os.path.exists(dir):
        try:
            os.makedirs(dir)
        except OSError, error:
            if error.errno != errno.EEXIST:
                raise

class FileAvoidWrite(StringIO):
    '''file-like object that buffers its output and only writes it to disk
    if the new contents are different from what the file may already contain.
    '''
    def __init__(self, filename):
        self.filename = filename
        StringIO.__init__(self)

    def close(self):
        buf = self.getvalue()
        StringIO.close(self)
        try:
            file = open(self.filename, 'rU')
        except IOError:
            pass
        else:
            try:
                 if file.read() == buf:
                     log("%s is unchanged" % relpath(self.filename, os.curdir))
                     return
            except IOError:
                pass
            finally:
                file.close()

        log("creating %s" % relpath(self.filename, os.curdir))
        ensureParentDir(self.filename)
        with open(self.filename, 'w') as file:
            file.write(buf)

    def __enter__(self):
        return self
    def __exit__(self, type, value, traceback):
        self.close()

def shell_escape(s):
    '''Escape some characters with a backslash, and double dollar signs.
    '''
    return re.sub('''([ \t`#$^&*(){}\\|;'"<>?\[\]])''', r'\\\1', str(s)).replace('$', '$$')

class ConfigEnvironment(object):
    '''A ConfigEnvironment is defined by a source directory and a build
    directory. It preprocesses files from the source directory and stores
    the result in the object directory.

    There are two types of files: config files and config headers,
    each treated through a different member function.

    Creating a ConfigEnvironment requires a few arguments:
      - topsrcdir and topobjdir are, respectively, the top source and
        the top object directory.
      - defines is a list of (name, value) tuples. In autoconf, these are
        set with AC_DEFINE and AC_DEFINE_UNQUOTED
      - non_global_defines are a list of names appearing in defines above
        that are not meant to be exported in ACDEFINES and ALLDEFINES (see
        below)
      - substs is a list of (name, value) tuples. In autoconf, these are
        set with AC_SUBST.

    ConfigEnvironment automatically defines two additional substs variables
    from all the defines not appearing in non_global_defines:
      - ACDEFINES contains the defines in the form -DNAME=VALUE, for use on
        preprocessor command lines. The order in which defines were given
        when creating the ConfigEnvironment is preserved.
      - ALLDEFINES contains the defines in the form #define NAME VALUE, in
        sorted order, for use in config files, for an automatic listing of
        defines.
    and another additional subst variable from all the other substs:
      - ALLSUBSTS contains the substs in the form NAME = VALUE, in sorted
        order, for use in autoconf.mk. It includes ACDEFINES, but doesn't
        include ALLDEFINES.

    ConfigEnvironment expects a "top_srcdir" subst to be set with the top
    source directory, in msys format on windows. It is used to derive a
    "srcdir" subst when treating config files. It can either be an absolute
    path or a path relative to the topobjdir.
    '''

    def __init__(self, topobjdir = '.', topsrcdir = '.',
                 defines = [], non_global_defines = [], substs = []):
        self.defines = dict(defines)
        self.substs = dict(substs)
        self.topsrcdir = topsrcdir
        self.topobjdir = topobjdir
        global_defines = [name for name, value in defines if not name in non_global_defines]
        self.substs['ACDEFINES'] = ' '.join(["-D%s=%s" % (name, shell_escape(self.defines[name])) for name in global_defines])
        self.substs['ALLSUBSTS'] = '\n'.join(sorted(["%s = %s" % (name, self.substs[name]) for name in self.substs]))
        self.substs['ALLDEFINES'] = '\n'.join(sorted(["#define %s %s" % (name, self.defines[name]) for name in global_defines]))

    def get_relative_srcdir(self, file):
        '''Returns the relative source directory for the given file, always
        using / as a path separator.
        '''
        assert(isinstance(file, basestring))
        dir = posixpath.dirname(relpath(file, self.topobjdir).replace(os.sep, '/'))
        if dir:
            return dir
        return '.'

    def get_top_srcdir(self, file):
        '''Returns a normalized top_srcdir for the given file: if
        substs['top_srcdir'] is a relative path, it is relative to the
        topobjdir. Adjust it to be relative to the file path.'''
        top_srcdir = self.substs['top_srcdir']
        if posixpath.isabs(top_srcdir) or ntpath.isabs(top_srcdir):
            return top_srcdir
        return posixpath.normpath(posixpath.join(self.get_depth(file), top_srcdir))

    def get_file_srcdir(self, file):
        '''Returns the srcdir for the given file, where srcdir is in msys
        format on windows, thus derived from top_srcdir.
        '''
        dir = self.get_relative_srcdir(file)
        top_srcdir = self.get_top_srcdir(file)
        return posixpath.normpath(posixpath.join(top_srcdir, dir))

    def get_depth(self, file):
        '''Returns the DEPTH for the given file, that is, the path to the
        object directory relative to the directory containing the given file.
        Always uses / as a path separator.
        '''
        return relpath(self.topobjdir, os.path.dirname(file)).replace(os.sep, '/')

    def get_input(self, file):
        '''Returns the input file path in the source tree that can be used
        to create the given config file or header.
        '''
        assert(isinstance(file, basestring))
        return os.path.normpath(os.path.join(self.topsrcdir, "%s.in" % relpath(file, self.topobjdir)))

    def create_config_file(self, path):
        '''Creates the given config file. A config file is generated by
        taking the corresponding source file and replacing occurences of
        "@VAR@" by the value corresponding to "VAR" in the substs dict.

        Additional substs are defined according to the file being treated:
            "srcdir" for its the path to its source directory
            "relativesrcdir" for its source directory relative to the top
            "DEPTH" for the path to the top object directory
        '''
        input = self.get_input(path)
        pp = Preprocessor()
        pp.context.update(self.substs)
        pp.context.update(top_srcdir = self.get_top_srcdir(path))
        pp.context.update(srcdir = self.get_file_srcdir(path))
        pp.context.update(relativesrcdir = self.get_relative_srcdir(path))
        pp.context.update(DEPTH = self.get_depth(path))
        pp.do_filter('attemptSubstitution')
        pp.setMarker(None)
        with FileAvoidWrite(path) as pp.out:
            pp.do_include(input)

    def create_config_header(self, path):
        '''Creates the given config header. A config header is generated by
        taking the corresponding source file and replacing some #define/#undef
        occurences:
            "#undef NAME" is turned into "#define NAME VALUE"
            "#define NAME" is unchanged
            "#define NAME ORIGINAL_VALUE" is turned into "#define NAME VALUE"
            "#undef UNKNOWN_NAME" is turned into "/* #undef UNKNOWN_NAME */"
            Whitespaces are preserved.
        '''
        with open(self.get_input(path), 'rU') as input:
            ensureParentDir(path)
            output = FileAvoidWrite(path)
            r = re.compile('^\s*#\s*(?P<cmd>[a-z]+)(?:\s+(?P<name>\S+)(?:\s+(?P<value>\S+))?)?', re.U)
            for l in input:
                m = r.match(l)
                if m:
                    cmd = m.group('cmd')
                    name = m.group('name')
                    value = m.group('value')
                    if name:
                        if name in self.defines:
                            if cmd == 'define' and value:
                                l = l[:m.start('value')] + str(self.defines[name]) + l[m.end('value'):]
                            elif cmd == 'undef':
                                l = l[:m.start('cmd')] + 'define' + l[m.end('cmd'):m.end('name')] + ' ' + str(self.defines[name]) + l[m.end('name'):]
                        elif cmd == 'undef':
                           l = '/* ' + l[:m.end('name')] + ' */' + l[m.end('name'):]

                output.write(l)
            output.close()

def config_status(topobjdir = '.', topsrcdir = '.',
                  defines = [], non_global_defines = [], substs = [],
                  files = [], headers = []):
    '''Main function, providing config.status functionality.

    Contrary to config.status, it doesn't use CONFIG_FILES or CONFIG_HEADERS
    variables, but like config.status from autoconf 2.6, single files may be
    generated with the --file and --header options. Several such options can
    be given to generate several files at the same time.

    Without the -n option, this program acts as config.status and considers
    the current directory as the top object directory, even when config.status
    is in a different directory. It will, however, treat the directory
    containing config.status as the top object directory with the -n option,
    while files given to the --file and --header arguments are considered
    relative to the current directory.

    The --recheck option, like with the original config.status, runs configure
    again, with the options given in the "ac_configure_args" subst.

    The options to this function are passed when creating the
    ConfigEnvironment, except for files and headers, which contain the list
    of files and headers to be generated by default. These lists, as well as
    the actual wrapper script around this function, are meant to be generated
    by configure. See build/autoconf/config.status.m4.

    Unlike config.status behaviour with CONFIG_FILES and CONFIG_HEADERS,
    but like config.status behaviour with --file and --header, providing
    files or headers on the command line inhibits the default generation of
    files when given headers and headers when given files.

    Unlike config.status, the FILE:TEMPLATE syntax is not supported for
    files and headers. The template is always the filename suffixed with
    '.in', in the corresponding directory under the top source directory.
    '''

    if 'CONFIG_FILES' in os.environ:
        raise Exception, 'Using the CONFIG_FILES environment variable is not supported. Use --file instead.'
    if 'CONFIG_HEADERS' in os.environ:
        raise Exception, 'Using the CONFIG_HEADERS environment variable is not supported. Use --header instead.'

    parser = OptionParser()
    parser.add_option('--recheck', dest='recheck', action='store_true',
                      help='update config.status by reconfiguring in the same conditions')
    parser.add_option('--file', dest='files', metavar='FILE', action='append',
                      help='instantiate the configuration file FILE')
    parser.add_option('--header', dest='headers', metavar='FILE', action='append',
                      help='instantiate the configuration header FILE')
    parser.add_option('-v', '--verbose', dest='verbose', action='store_true',
                      help='display verbose output')
    parser.add_option('-n', dest='not_topobjdir', action='store_true',
                      help='do not consider current directory as top object directory')
    (options, args) = parser.parse_args()

    # Without -n, the current directory is meant to be the top object directory
    if not options.not_topobjdir:
        topobjdir = '.'

    env = ConfigEnvironment(topobjdir = topobjdir, topsrcdir = topsrcdir,
                            defines = defines, non_global_defines = non_global_defines,
                            substs = substs)

    if options.recheck:
        # Execute configure from the top object directory
        if not os.path.isabs(topsrcdir):
            topsrcdir = relpath(topsrcdir, topobjdir)
        os.chdir(topobjdir)
        os.execlp('sh', 'sh', '-c', ' '.join([os.path.join(topsrcdir, 'configure'), env.substs['ac_configure_args'], '--no-create', '--no-recursion']))

    if options.files:
        files = options.files
        headers = []
    if options.headers:
        headers = options.headers
        if not options.files:
            files = []
    # Default to display messages when giving --file or --headers on the
    # command line.
    if options.files or options.headers or options.verbose:
        global verbose
        verbose = True
    if not options.files and not options.headers:
        print >>sys.stderr, "creating config files and headers..."
        files = [os.path.join(topobjdir, f) for f in files]
        headers = [os.path.join(topobjdir, f) for f in headers]

    for file in files:
        env.create_config_file(file)
    for header in headers:
        env.create_config_header(header)
