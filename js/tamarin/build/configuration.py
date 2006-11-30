import os
import sys
import build.process
import re

# Figure out TARGET and CPU, a la config.guess
# Do cross-compilation in the future, which will require HOST_OS and perhaps
# other settings

def _configGuess():
    ostest = sys.platform
    if re.match('^win', ostest):
        os = 'windows'
    elif ostest == 'darwin':
        os = 'darwin'
    elif ostest.search('^linux', ostest):
        os = 'linux'
    else:
	raise Exception('Unrecognized OS: ' + ostest)

    cputest = build.process.run_for_output(['uname', '-m'])
    if re.search(r'^i(\d86|86pc|x86)$', cputest):
        cpu = 'i686'
    elif re.search('^(x86_64|amd64)$', cputest):
        cpu = 'x86_64'
    elif re.search('^(ppc|powerpc)$', cputest):
        cpu = 'powerpc'
    else:
	raise Exception('Unrecognized CPU: ' + cputest)

    return (os, cpu)

class Configuration:
    def __init__(self, topsrcdir, options=None, sourcefile=None, objdir=None,
                 optimize=True, debug=False):
        self._topsrcdir = topsrcdir
        if objdir:
            self._objdir = objdir
        else:
            self._objdir = os.getcwd()

        self._optimize = optimize
        self._debug = debug
        self._host = _configGuess()
        self._target = self._host

        if sourcefile:
            srcfile = self._topsrcdir + "/" + sourcefile
            if not os.path.exists(srcfile):
                raise Exception("Source file " + srcfile + " doesn't exist.")

        objfile = self._objdir + "/" + sourcefile
        if os.path.exists(objfile):
            raise Exception("Object file " + objfile + " exists, and shouldn't. You must use an object directory to build Tamarin.")

        if options:
            o = options.getStringArg("optimize")

	    if o != None:
		self._optimize = o

            d = options.getStringArg("debug")
	    if d != None:
		self._debug = d

	    if options.host:
		self._host = _configSub(options.host)

	    if options.target:
		self._target = _configSub(options.target)

        self._acvars = {
            'topsrcdir': self._topsrcdir,
	    'HOST_OS': self._host[0],
            'TARGET_OS': self._target[0],
            'TARGET_CPU': self._target[1]
            }

	if self._debug:
	    self._acvars['ENABLE_DEBUG'] = 1

    def getHost(self):
	"""Returns an (os, cpu) tuple of the host machine."""
	return self._host

    def getTarget(self):
	"""Returns an (os, cpu) tuple of the target machine."""
	return self._target

    def getCompiler(self):
	self.COMPILER_IS_GCC = True
	self._acvars.update({
	    'OBJ_SUFFIX': 'o',
	    'LIB_PREFIX': 'lib',
	    'LIB_SUFFIX': 'a',
	    'DLL_SUFFIX': 'so',
	    'PROGRAM_SUFFIX': '',
	    'USE_COMPILER_DEPS': 1,
	    'OS_LIBS'   : 'z',
	    'EXPAND_LIBNAME' : '-l$(1)'
	    })

	if self._target[0] == 'windows':
	    self.COMPILER_IS_GCC = False
	    del self._acvars['USE_COMPILER_DEPS']
	    self._acvars.update({
	        'OBJ_SUFFIX'   : 'obj',
		'LIB_PREFIX'   : '',
		'LIB_SUFFIX'   : 'lib',
		'DLL_SUFFIX'   : 'dll',
		'PROGRAM_SUFFIX': '.exe',
		'CPPFLAGS'     : '-MT',
		'CXX'          : 'cl.exe',
		'CXXFLAGS'     : '',
		'AR'           : 'lib.exe',
		'LD'           : 'link.exe',
		'LDFLAGS'      : '',
		'MKSTATICLIB'  : '$(AR) -OUT:$(1)',
		'MKPROGRAM'    : '$(LD) -OUT:$(1)',
		'EXPAND_LIBNAME' : '$(1).lib'
		})
	    if debug:
		self._acvars['CPPFLAGS'] = '-MTD'

	# Hackery! Make assumptions that we want to build with GCC 3.3 on MacPPC
	# and GCC4 on MacIntel
	elif self._target[0] == 'darwin':
	    self._acvars.update({
	        'DLL_SUFFIX'   : 'dylib',
		'CPPFLAGS'     : '-pipe',
		'CXXFLAGS'     : '',
		'LDFLAGS'      : '-lz -framework CoreServices',
		'AR'           : 'ar',
		'MKSTATICLIB'  : '$(AR) cr $(1)',
		'MKPROGRAM'    : '$(CXX) -o $(1)'
		})

# -Wno-trigraphs -Wreturn-type -Wnon-virtual-dtor -Wmissing-braces -Wparentheses -Wunused-label  -Wunused-parameter -Wunused-variable -Wunused-value -Wuninitialized

	    if self._target[1] == 'i686':
		self._acvars['CXX'] = 'g++-4.0'
	    elif self._target[1] == 'powerpc':
		self._acvars['CXX'] = 'g++-3.3'
	    else:
		raise Exception("Unexpected Darwin processor.")

	elif self._target[0] == 'linux':
	    self._acvars.update({
		'CPPFLAGS'     : '',
		'CXX'          : 'g++',
		'CXXFLAGS'     : '',
		'LD'           : 'ar',
		'LDFLAGS'      : '',
		'MKSTATICLIB'  : '$(AR) cr $(1)',
		'MKPROGRAM'    : '$(CXX) -o $(1)'
		})

    def subst(self, name, value):
	self._acvars[name] = value

    _confvar = re.compile("@([^@]+)@")

    def generate(self, makefile):
	print "Generating " + makefile + "...",
	outpath = self._objdir + "/" + makefile

	contents = \
	    "\n".join([k + "=" + str(v) \
		       for (k,v) in self._acvars.iteritems()]) + \
	    "\n\ninclude $(topsrcdir)/build/config.mk\n" \
	    "include $(topsrcdir)/manifest.mk\n" \
	    "include $(topsrcdir)/build/rules.mk\n"

	try:
	    outf = open(outpath, "r")
	    oldcontents = outf.read()
	    outf.close()

	    if oldcontents == contents:
		print "not changed"
		return
	except IOError:
	    pass

	outf = open(outpath, "w")
	outf.write(contents)
	outf.close()
	print
