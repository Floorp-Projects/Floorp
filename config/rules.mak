# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

!if !defined(VERBOSE)
.SILENT:
!endif
#//------------------------------------------------------------------------
#//
#// This makefile contains all of the common rules shared by all other
#// makefiles.
#//
#//------------------------------------------------------------------------

!if !defined(CONFIG_RULES_MAK)
CONFIG_RULES_MAK=1


#//------------------------------------------------------------------------
#//  Assumed variables by the manifest.
#//------------------------------------------------------------------------
!if !defined(PACKAGE)
PACKAGE=.
!endif # PACKAGE

!if !defined(JDK_GEN_DIR)
JDK_GEN_DIR=_gen
!endif

!if !defined(JDK_STUB_DIR)
JDK_STUB_DIR=_stubs
!endif

!if !defined(JMC_GEN_DIR)
!if defined(JAVA_OR_NSJVM)
JMC_GEN_DIR=_jmc
!else
JMC_GEN_DIR=$(LOCAL_JMC_SUBDIR)
!endif
!endif

!if !defined(JRI_GEN_DIR)
JRI_GEN_DIR=_jri
!endif

!if !defined(JNI_GEN_DIR)
JNI_GEN_DIR=_jni
!endif


#//------------------------------------------------------------------------
#//  Make sure that JDIRS is set after the manifest file is included
#//  and before the rules for JDIRS get generated. We cannot put this line
#//  in the makefile.win after including rules.mak as the rules would already
#//  be generated based on JDIRS set in manifest.mn. We cannot put in ifdefs in
#//  manifest.mn too I was told.
#//------------------------------------------------------------------------

!ifdef JDIRS
JDIRS=$(JDIRS) $(JSCD)
!if "$(STAND_ALONE_JAVA)" == "1"
JDIRS=$(JDIRS) $(SAJDIRS)
!endif
!endif


!if "$(MOZ_BITS)" == "16"
#//------------------------------------------------------------------------
#//  All public win16 headers go to a single directory
#//	due to compiler limitations.
#//------------------------------------------------------------------------
MODULE=win16
!endif # 16


OBJS=$(OBJS) $(C_OBJS) $(CPP_OBJS)

include <$(DEPTH)/config/config.mak>

!if "$(WINOS)" == "WIN95"
_NO_FLOCK=-l
!else
_NO_FLOCK=
!endif

#//------------------------------------------------------------------------
#//
#// Definitions for building components. A ``component'' is a module
#// that has an NSGetModule entry point that can be called to enumerate
#// the XPCOM components contained in the module. A component can either
#// be built as a stand-alone DLL, or as a static library which can be
#// linked with other components to form a ``meta-component'' or included
#// in a final executable.
#//
#//   MODULE_NAME
#//     If set, indicates that we're building a ``component''. This value
#//     should be set to the name of the generic module (as declared by
#//     the NS_IMPL_NSGENERICMODULE macro; e.g., ``nsLayoutModule'').
#//
#//   LIBRARY_NAME
#//     For a component, the name of the library that will be generated;
#//     e.g., ``gklayout''.
#//
#//   META_COMPONENT
#//     If set, the component is included in the packaging list for the
#//     specified meta-component; if unset, the component is linked into
#//     the final executable. This is only meaningful during a static
#//     build.
#//
#//   SUB_LIBRARIES
#//     If the component is comprised of static libraries, then this
#//     lists those libraries.
#//
#//   LLIBS
#//     Any extra library dependencies that are required when the component
#//     is built as a DLL.
#//
#// When doing a ``dynamic build'', the component will be linked as a stand-
#// alone DLL which will be installed in the $(DIST)/bin/components directory.
#// No import library will be created.
#//
#// When doing a ``static build'', the component will be linked into a
#// static library which is installed in the $(DIST)/lib directory, and
#// either linked with the appropriate META_COMPONENT DLL, or the final
#// executable if no META_COMPONENT is set.
#//
#//------------------------------------------------------------------------
!if defined(MODULE_NAME)
# We're building a component
!if defined(EXPORT_LIBRARY)
!error "Can't define both MODULE_NAME and EXPORT_LIBRARY."
!endif

!if defined(MOZ_STATIC_COMPONENT_LIBS)
MAKE_OBJ_TYPE=$(NULL)

!if defined(META_COMPONENT)
META_LINK_COMPS=$(DIST)\$(META_COMPONENT)-link-comps
META_LINK_COMP_NAMES=$(DIST)\$(META_COMPONENT)-link-comp-names
!endif

LIBRARY=.\$(OBJDIR)\$(LIBRARY_NAME).lib

!else

# Build the component as a standalone DLL
MAKE_OBJ_TYPE=DLL

DLL=.\$(OBJDIR)\$(LIBRARY_NAME).dll

LLIBS=$(SUB_LIBRARIES) $(LLIBS)
!endif

!endif

#//------------------------------------------------------------------------
#//
#// Definitions for building top-level export libraries.
#//
#//   EXPORT_LIBRARY
#//     If set (typically to ``1''), indicates that we're building a
#//      ``top-level export library''.
#//
#//   LIBRARY_NAME
#//     Set to the name of the library.
#//
#//   META_COMPONENT
#//     If set, the name of the meta-component to which this export
#//     library belongs. If unset, the export library is linked with
#//     the final executable.
#//
#//------------------------------------------------------------------------
!if defined(EXPORT_LIBRARY)
# We're building a top-level, non-component library

!if defined(MOZ_STATIC_COMPONENT_LIBS)

# Build it as a static lib, not a DLL
MAKE_OBJ_TYPE=$(NULL)

!if defined(META_COMPONENT)
META_LINK_LIBS=$(DIST)\$(META_COMPONENT)-link-libs
!endif

LIBRARY=.\$(OBJDIR)\$(LIBRARY_NAME).lib

!else
# Build the library as a standalone DLL
MAKE_OBJ_TYPE=DLL

DLL=.\$(OBJDIR)\$(LIBRARY_NAME).dll

LLIBS=$(SUB_LIBRARIES) $(LLIBS)
!endif

!endif

#//------------------------------------------------------------------------
#//
#// Definitions for miscellaneous libraries that are not components or
#// top-level export libraries.
#//
#//   LIBRARY_NAME
#//     The name of the library to be created.
#//
#//------------------------------------------------------------------------
!if defined(LIBRARY_NAME) && !defined(MODULE_NAME) && !defined(EXPORT_LIBRARY)
!if !defined(LIBRARY)
LIBRARY=$(OBJDIR)\$(LIBRARY_NAME).lib
!endif
!endif

#//------------------------------------------------------------------------
#//
#// Specify a default target if non was set...
#//
#//------------------------------------------------------------------------
!ifndef TARGETS
TARGETS=$(PROGRAM) $(LIBRARY) $(DLL)
!endif

!ifndef MAKE_ARGS
#MAKE_ARGS=all
!endif

!if "$(WINOS)" == "WIN95"
W95MAKE=$(DEPTH)\config\w95make.exe
W32OBJS = $(OBJS:.obj=.obj, )
W32LOBJS = $(OBJS: .= +-.)
!endif

all::
    $(NMAKE) -f makefile.win export
    $(NMAKE) -f makefile.win install

#//------------------------------------------------------------------------
#//
#// Setup tool flags for the appropriate type of objects being built
#// (either DLL or EXE)
#//
#//------------------------------------------------------------------------
!if "$(MAKE_OBJ_TYPE)" == "DLL"
CFLAGS=$(DLL_CFLAGS) $(CFLAGS)
LFLAGS=$(DLL_LFLAGS) $(LFLAGS)
OS_LIBS=$(DLL_LIBS) $(OS_LIBS)
!else
CFLAGS=$(EXE_CFLAGS) $(CFLAGS)
LFLAGS=$(EXE_LFLAGS) $(LFLAGS)
OS_LIBS=$(EXE_LIBS) $(OS_LIBS)
!endif

#//------------------------------------------------------------------------
#//
#// Use various library names as default name for PDB Files
#//
#// LIBRARY_NAME - Static Library
#// DLLNAME - Dynamic Load Library
#//
#//
#//------------------------------------------------------------------------

# Replace optimizer and pdb related flags to use our own conventions
!ifdef LIBRARY_NAME
PDBFILE=$(LIBRARY_NAME)
!endif

# Replace optimizer and pdb related flags to use our own conventions
!ifdef DLLNAME
PDBFILE=$(DLLNAME)
!endif

#//------------------------------------------------------------------------
#//
#// Prepend the "object directory" to any public make variables.
#//    PDBFILE - File containing debug info
#//    RESFILE - Compiled resource file
#//    MAPFILE - MAP file for an executable
#//
#//------------------------------------------------------------------------
!ifdef PDBFILE
PDBFILE=.\$(OBJDIR)\$(PDBFILE).pdb
!else
PDBFILE=.\$*.pdb  # used for executables
!endif
!ifdef RESFILE
RESFILE=.\$(OBJDIR)\$(RESFILE)
!endif
!ifdef MAPFILE
MAPFILE=.\$(OBJDIR)\$(MAPFILE)
!endif

!ifdef DIRS
#//------------------------------------------------------------------------
#//
#// Rule to recursively make all subdirectories specified by the DIRS target
#//
#//------------------------------------------------------------------------
$(DIRS)::
!if "$(WINOS)" == "WIN95"
!if defined(VERBOSE)
    @echo +++ make: cannot recursively make on win95 using command.com, use w95make.
!endif
!else
    @echo +++ make: %MAKE_ARGS% in $(MAKEDIR)\$@
	@cd $@
	@$(NMAKE) -f makefile.win %%MAKE_ARGS%%
    @cd $(MAKEDIR) 
!endif

!endif # DIRS

#//------------------------------------------------------------------------
#//
#// Created directories
#//
#//------------------------------------------------------------------------


$(JAVA_DESTPATH):
!if "$(AWT_11)" == "1"
    -mkdir $(XPDIST:/=\)\classes11
!else
    -mkdir $(XPDIST:/=\)\classes
!endif

$(JAVA_DESTPATH)\$(PACKAGE): $(JAVA_DESTPATH)
!if "$(AWT_11)" == "1"
    -mkdir $(XPDIST:/=\)\classes11\$(PACKAGE:/=\)
!else
    -mkdir $(XPDIST:/=\)\classes\$(PACKAGE:/=\)
!endif
$(JMCSRCDIR):
    -mkdir $(JMCSRCDIR)

$(PUBLIC):
    -mkdir $(XPDIST:/=\)\include

!ifdef IDL_GEN
#//------------------------------------------------------------------------
#//
#// IDL Stuff
#//
#//------------------------------------------------------------------------

idl::
	@echo +++ make: Starting osagent
	@start $(DEPTH)\modules\iiop\tools\win32\osagent
	@echo +++ make: idl2java $(IDL_GEN)
	@type <<cmd.cfg
$(IDL2JAVA_FLAGS) $(IDL_GEN)
<<
	@$(IDL2JAVA_PROG) -argfile cmd.cfg
	@del cmd.cfg

!endif # IDL_GEN

TMPDIR=$(MOZ_SRC)\tmp
$(TMPDIR):
	-mkdir $(TMPDIR)

!if defined(INSTALL_FILE_LIST) && defined(INSTALL_DIR)
#//------------------------------------------------------------------------
#//
#// Rule to install the files specified by the INSTALL_FILE_LIST variable
#// into the directory specified by the INSTALL_DIR variable
#//
#//------------------------------------------------------------------------
!if "$(MOZ_BITS)" == "16"
#//------------------------------------------------------------------------
#//  All public win16 headers go to a single directory
#//	due to compiler limitations.
#//------------------------------------------------------------------------
INSTALL_DIR=$(PUBLIC)\win16
!endif # 16
INSTALL_FILES: $(INSTALL_FILE_LIST)
    !$(MAKE_INSTALL) $** $(INSTALL_DIR)

!endif # INSTALL_FILES

#//------------------------------------------------------------------------
#//
#// Global rules...
#//
#//------------------------------------------------------------------------

#//------------------------------------------------------------------------
#//
#// Rules for building components
#//
#//------------------------------------------------------------------------
!if defined(MODULE_NAME)
# We're building a component
!if defined(EXPORT_LIBRARY)
!error "Can't define both MODULE_NAME and EXPORT_LIBRARY."
!endif

!if defined(MOZ_STATIC_COMPONENT_LIBS)

# We're building this component as a static lib
!if defined(META_COMPONENT)

# It's to be linked into a meta-component. Add the component name to
# the meta component's list
export::
        $(PERL) -I$(DEPTH)\config $(DEPTH)\config\build-list.pl $(_NO_FLOCK) $(META_LINK_COMPS:\=/) $(LIBRARY_NAME)
        $(PERL) -I$(DEPTH)\config $(DEPTH)\config\build-list.pl $(_NO_FLOCK) $(META_LINK_COMP_NAMES:\=/) $(MODULE_NAME)

!else # defined(META_COMPONENT)
# Otherwise, it's to be linked into the main executable. Add the component
# name to the list of components, and the library name to the list of
# static libs.
export::
        $(PERL) -I$(DEPTH)\config $(DEPTH)\config\build-list.pl $(_NO_FLOCK) $(FINAL_LINK_COMPS:\=/) $(LIBRARY_NAME)
        $(PERL) -I$(DEPTH)\config $(DEPTH)\config\build-list.pl $(_NO_FLOCK) $(FINAL_LINK_COMP_NAMES:\=/) $(MODULE_NAME)

!endif # defined(META_COMPONENT)

install:: $(LIBRARY)
        $(MAKE_INSTALL) $(LIBRARY) $(DIST)\lib

clobber::
        $(RM) $(DIST)\lib\$(LIBRARY_NAME).lib

!else

# Build the component as a standalone DLL. Do _not_ install the import
# library, because it's a component; nobody should be linking against
# it!

install:: $(DLL)
        $(MAKE_INSTALL) $(DLL) $(DIST)\bin\components

clobber::
        $(RM) $(DIST)\bin\components\$(DLL)

!endif

!endif

#//------------------------------------------------------------------------
#//
#// Rules for building top-level export libraries
#//
#//------------------------------------------------------------------------
!if defined(EXPORT_LIBRARY)
# We're building a top-level, non-component library

!if defined(MOZ_STATIC_COMPONENT_LIBS)

!if defined(META_COMPONENT)
# It's to be linked into a meta-component. Add the library to the
# meta component's list

export::
        $(PERL) -I$(DEPTH)\config $(DEPTH)\config\build-list.pl $(_NO_FLOCK) $(META_LINK_LIBS:\=/) $(LIBRARY_NAME)

!else # defined(META_COMPONENT)
# Otherwise, it's to be linked into the main executable. Add the
# library to the list of static libs.

export::
        $(PERL) -I$(DEPTH)\config $(DEPTH)\config\build-list.pl $(_NO_FLOCK) $(FINAL_LINK_LIBS:\=/) $(LIBRARY_NAME)

!endif # defined(META_COMPONENT)

install:: $(LIBRARY)
        $(MAKE_INSTALL) $(LIBRARY) $(DIST)\lib

clobber::
        $(RM) $(DIST)\lib\$(LIBRARY_NAME).lib

!else

# Build the library as a standalone DLL. We _will_ install the import
# library in this case, because people may link against it.

install:: $(DLL) $(OBJDIR)\$(LIBRARY_NAME).lib
        $(MAKE_INSTALL) $(DLL) $(DIST)\bin
        $(MAKE_INSTALL) $(OBJDIR)\$(LIBRARY_NAME).lib $(DIST)\lib

clobber::
        $(RM) $(DIST)\bin\$(DLL)
        $(RM) $(DIST)\lib\$(LIBRARY_NAME).lib

!endif

!endif

#//------------------------------------------------------------------------
#//
#// Rules for miscellaneous libraries
#//
#//------------------------------------------------------------------------
!if defined(LIBRARY)

install:: $(LIBRARY)
        $(MAKE_INSTALL) $(LIBRARY) $(DIST)/lib

clobber::
        rm -f $(DIST)/lib/$(LIBRARY_NAME).lib

!endif


#//------------------------------------------------------------------------
#//
#// Rules for recursion
#//
#//----------------------------------------------------------------------

#//
#// Set the MAKE_ARGS variable to indicate the target being built...  This is used
#// when processing subdirectories via the $(DIRS) rule
#//


#
# Nasty hack to get around the win95 shell's inability to set 
# environment variables whilst in a set of target commands
#
!if "$(WINOS)" == "WIN95"

clean:: 
!ifdef DIRS
     @$(W95MAKE) clean $(MAKEDIR) $(DIRS)
!endif
    -$(RM) $(OBJS) $(NOSUCHFILE) NUL 2> NUL

clobber:: 
!ifdef DIRS
     @$(W95MAKE) clobber $(MAKEDIR) $(DIRS)
!endif

clobber_all::
!ifdef DIRS
     @$(W95MAKE) clobber_all $(MAKEDIR) $(DIRS)
!endif
  
export::
!ifdef DIRS
    @$(W95MAKE) export $(MAKEDIR) $(DIRS)
!endif # DIRS

install::
!ifdef DIRS
    @$(W95MAKE) install $(MAKEDIR) $(DIRS)
!endif # DIRS

depend::
!ifdef DIRS
    @$(W95MAKE) depend $(MAKEDIR) $(DIRS)
!endif # DIRS

mangle::
!ifdef DIRS
    @$(W95MAKE) mangle $(MAKEDIR) $(DIRS)
!endif # DIRS
    $(MAKE_MANGLE)

unmangle::
!ifdef DIRS
    @$(W95MAKE) unmangle $(MAKEDIR) $(DIRS)
!endif # DIRS
    -$(MAKE_UNMANGLE)

!else


clean:: 
	@set MAKE_ARGS=$@

clobber:: 
	@set MAKE_ARGS=$@

clobber_all:: 
	@set MAKE_ARGS=$@

export:: 
	@set MAKE_ARGS=$@

libs:: 
	@set MAKE_ARGS=$@

install:: 
	@set MAKE_ARGS=$@

mangle:: 
	@set MAKE_ARGS=$@

unmangle:: 
	@set MAKE_ARGS=$@

depend:: 
	@set MAKE_ARGS=$@

!endif

#//------------------------------------------------------------------------
#// DEPEND
#//
#//   If you ever need to change the set of includes (e.g., you go whack
#//   the build rules over in obj.inc), make sure to update the input
#//   to $(MAKEDEP). It only looks through the directories that you tell
#//   it to.
#//
#//------------------------------------------------------------------------

MAKEDEP=$(DEPTH)\config\makedep.exe
MAKEDEPFILE=.\$(OBJDIR:/=\)\make.dep

MAKEDEPDETECT=$(OBJS)
MAKEDEPDETECT=$(MAKEDEPDETECT: =)
MAKEDEPDETECT=$(MAKEDEPDETECT:	=)

!if !defined(NODEPEND) && "$(MAKEDEPDETECT)" != ""

depend:: $(OBJDIR)
    @echo Analyzing dependencies...
    $(MAKEDEP) -s -o $(MAKEDEPFILE) @<<
$(LINCS)
$(LINCS_1)
$(INCS)
-I$(MAKEDIR)
$(OBJS)
<<

!endif

!IF EXIST($(MAKEDEPFILE)) 
!INCLUDE <$(MAKEDEPFILE)>
!ENDIF

export:: $(DIRS)

libs:: 
    @echo The libs build phase is obsolete.

install:: $(DIRS)

depend:: $(DIRS)

mangle:: $(DIRS)
    $(MAKE_MANGLE)

unmangle:: $(DIRS)
    -$(MAKE_UNMANGLE)


alltags::
        @echo +++ Making emacs tags
	c:\\mksnt\\find . -name dist -prune -o ( -name '*.[hc]' -o -name '*.cpp' -o -name '*.idl' ) -print | c:\\mksnt\\xargs etags -a

echo-dirs:
	@echo.$(DIRS)

echo-module:
	@echo.$(MODULE)

echo-requires:
	@echo.$(REQUIRES)

echo-incs:
	@echo.$(INCS)

#//------------------------------------------------------------------------
#//
#// Rule to create the object directory (if necessary)
#//
#//------------------------------------------------------------------------
$(OBJDIR):
	@echo +++ make: Creating directory: $(OBJDIR)
    -mkdir $(OBJDIR)


#//------------------------------------------------------------------------
#//
#// Include the makefile for building the various targets...
#//
#//------------------------------------------------------------------------
include <$(DEPTH)/config/obj.inc>
include <$(DEPTH)/config/exe.inc>
include <$(DEPTH)/config/dll.inc>
include <$(DEPTH)/config/lib.inc>
include <$(DEPTH)/config/java.inc>


#//------------------------------------------------------------------------
#//
#// EXPORTS
#//
#// Names of headers to be copied to common include directory
#//
#//------------------------------------------------------------------------
!if "$(EXPORTS)" != "$(NULL)"

export:: $(EXPORTS)
    @echo +++ make: exporting headers
 	$(MAKE_INSTALL:/=\) $(MKCPYFLAGS) $(EXPORTS) $(PUBLIC)
	$(PERL) -I$(DEPTH)\config $(DEPTH)\config\build-list.pl $(PUBLIC)/.headerlist $(EXPORTS)

#// don't delete exported stuff on a local clobber, use clobber_all
#clobber::
#!if exist($(PUBLIC))
#    @cd $(PUBLIC)
#    -$(RM) $(EXPORTS)
#    @cd $(MAKEDIR)
#!endif # $(PUBLIC) exists

clobber_all::
!if exist($(PUBLIC))
    @cd $(PUBLIC)
    -$(RM) $(EXPORTS)
    @cd $(MAKEDIR)
!endif # $(PUBLIC) exists

!endif # EXPORTS

#//------------------------------------------------------------------------
#//  These rules must follow all lines that define the macros they use
#//------------------------------------------------------------------------
!if defined(JAVA_OR_NSJVM)
GARBAGE	= $(GARBAGE) $(JMC_GEN_DIR) $(JMC_HEADERS) $(JMC_STUBS) \
	  $(JDK_STUB_DIR) $(JRI_GEN_DIR) $(JDK_GEN_DIR) $(JNI_GEN_DIR) 
!endif


clean:: $(DIRS)
    -$(RM) $(OBJS) $(NOSUCHFILE) NUL 2> NUL

clobber:: $(DIRS)
!if defined(GARBAGE) || exist($(OBJDIR))
    -$(RM_R) $(GARBAGE) $(OBJDIR) 2> NUL
!endif

clobber_all:: $(DIRS)
!if defined(GARBAGE) || "$(TARGETS)" != "  " || exist($(OBJDIR))
    -$(RM_R) $(TARGETS) $(GARBAGE) $(OBJDIR) 2> NUL
!endif

!if "$(MOZ_BITS)"=="32"
CFLAGS = $(CFLAGS) -DNO_JNI_STUBS
!endif


#//------------------------------------------------------------------------
#//  XPIDL rules
#//------------------------------------------------------------------------
!if "$(XPIDLSRCS)" != "$(NULL)"
!if "$(MODULE)" != "$(NULL)"

# Generate header files and type libraries from the XPIDLSRCS variable.

.SUFFIXES: .idl .xpt

# XXX Note the direct use of '.\_xpidlgen' instead of
# $(XPIDL_GEN_DIR). 'nmake' is too stupid to deal with recursive macro
# substitution.

XPIDL_INCLUDES=$(XPIDL_INCLUDES) -I$(XPDIST)\idl

XPIDL_HEADERS=$(XPIDLSRCS:.idl=.h)
XPIDL_HEADERS=$(XPIDL_HEADERS:.\=.\_xpidlgen\)

!ifndef NO_GEN_XPT
XPIDL_TYPELIBS=$(XPIDLSRCS:.idl=.xpt)
XPIDL_TYPELIBS=$(XPIDL_TYPELIBS:.\=.\_xpidlgen\)

.idl{$(XPIDL_GEN_DIR)}.xpt:
        $(XPIDL_PROG) -m typelib -w $(XPIDL_INCLUDES) -o $* $<

!ifndef XPIDL_MODULE
XPIDL_MODULE = $(MODULE)
!endif

TYPELIB = $(XPIDL_GEN_DIR)\$(XPIDL_MODULE).xpt

$(TYPELIB): $(XPIDL_TYPELIBS) $(XPTLINK_PROG)
        @echo +++ make: Creating typelib: $(TYPELIB)
        $(XPTLINK_PROG) $(TYPELIB) $(XPIDL_TYPELIBS)
!endif

$(XPIDL_GEN_DIR):
	@echo +++ make: Creating directory: $(XPIDL_GEN_DIR)
	-mkdir $(XPIDL_GEN_DIR)

$(XPIDL_HEADERS): $(XPIDL_PROG)

.idl{$(XPIDL_GEN_DIR)}.h:
        $(XPIDL_PROG) -m header -w $(XPIDL_INCLUDES) -o $* $<

$(DIST)\include:
	@echo +++ make: Creating directory: $(DIST)\include
	-mkdir $(DIST)\include

$(XPDIST)\idl:
        @echo +++ make: Creating directory: $(XPDIST)\idl
        -mkdir $(XPDIST)\idl

export:: $(XPDIST)\idl
        @echo +++ make: exporting IDL files
        $(MAKE_INSTALL) $(XPIDLSRCS:/=\) $(XPDIST)\idl

export:: $(XPIDL_GEN_DIR) $(XPIDL_HEADERS) $(PUBLIC)
        @echo +++ make: exporting generated XPIDL header files
        $(MAKE_INSTALL) $(XPIDL_HEADERS:/=\) $(PUBLIC)
	$(PERL) -I$(DEPTH)\config $(DEPTH)\config\build-list.pl $(PUBLIC)/.headerlist $(XPIDL_HEADERS)

!ifndef NO_GEN_XPT
install:: $(XPIDL_GEN_DIR) $(TYPELIB)
        @echo +++ make: installing typelib '$(TYPELIB)' to components directory
        $(MAKE_INSTALL) $(TYPELIB) $(DIST)\bin\components
!endif

clobber::
        -$(RM_R) $(XPIDL_GEN_DIR) 2> NUL
        
clobber_all::
        -$(RM_R) $(XPIDL_GEN_DIR) 2> NUL
!if exist($(PUBLIC))
        @cd $(PUBLIC)
        -$(RM) $(XPIDLSRCS:.idl=.h)
        @cd $(MAKEDIR)
!endif
!if exist($(XPDIST:/=\)\idl)
        @cd $(XPDIST:/=\)\idl
        -$(RM) $(XPIDLSRCS)
        @cd $(MAKEDIR)
!endif
!if exist($(DIST)\bin\components)
        -$(RM) $(DIST)\bin\components\$(XPIDL_MODULE).xpt
!endif

!endif
!endif

#//----------------------------------------------------------------------
#//
#// Component packaging rules
#//
#//----------------------------------------------------------------------
$(FINAL_LINK_COMPS):
        @echo +++ make: creating file: $(FINAL_LINK_COMPS)
        @echo. > $(FINAL_LINK_COMPS)

$(FINAL_LINK_COMP_NAMES):
        @echo +++ make: creating file: $(FINAL_LINK_COMP_NAMES)
        @echo. > $(FINAL_LINK_COMP_NAMES)

$(FINAL_LINK_LIBS):
        @echo +++ make: creating file: $(FINAL_LINK_LIBS)
        @echo. > $(FINAL_LINK_LIBS)


################################################################################
## CHROME PACKAGING

JAR_MANIFEST = jar.mn

chrome::

install:: chrome

!ifdef MOZ_CHROME_FILE_FORMAT
_CHROME_FILE_FORMAT=$(MOZ_CHROME_FILE_FORMAT)

!if "$(_CHROME_FILE_FORMAT)" == "flat"
_JAR_REGCHROME_DISABLE_JAR=1
!else
_JAR_REGCHROME_DISABLE_JAR=0
!endif

!else

!ifdef MOZ_DISABLE_JAR_PACKAGING
_CHROME_FILE_FORMAT=flat
_JAR_REGCHROME_DISABLE_JAR=1
!else
_CHROME_FILE_FORMAT=jar
_JAR_REGCHROME_DISABLE_JAR=0
!endif

!endif

REGCHROME = @perl -I$(DEPTH)\config $(DEPTH)\config\add-chrome.pl $(_NO_FLOCK) $(DIST)\bin\chrome\installed-chrome.txt $(_JAR_REGCHROME_DISABLE_JAR)

!ifndef MOZ_OLD_JAR_PACKAGING

!if exist($(JAR_MANIFEST))

chrome:: $(CHROME_DEPS)
        $(PERL) -I$(DEPTH)\config $(DEPTH)\config\make-jars.pl -f $(_CHROME_FILE_FORMAT) $(_NO_FLOCK) -d $(DIST)\bin\chrome < $(JAR_MANIFEST)
#        $(PERL) -I$(DEPTH)\config $(DEPTH)\config\make-chromelist.pl $(DIST)\bin\chrome $(JAR_MANIFEST) $(_NO_FLOCK)
!endif

regchrome:

!else # !MOZ_OLD_JAR_PACKAGING

################################################################################
# Generate chrome building rules.
#
# You need to set these in your makefile.win to utilize this support:
#   CHROME_DIR - specifies the chrome subdirectory where your chrome files
#                go; e.g., CHROME_DIR=navigator or CHROME_DIR=global
#
# Note:  All file listed in the next three macros MUST be prefaced with .\ (or ./)!
#
#   CHROME_CONTENT - list of chrome content files; these can be prefaced with
#                arbitrary paths; e.g., CHROME_CONTENT=./content/default/foobar.xul
#   CHROME_SKIN - list of skin files
#   CHROME_L10N - list of localization files, e.g., CHROME_L10N=./locale/en-US/foobar.dtd
#
# These macros are optional, if not specified, each defaults to ".".
#   CHROME_CONTENT_DIR - Specifies a subdirectory within CHROME_DIR where
#                  all CHROME_CONTENT files will be installed.
#   CHROME_SKIN_DIR - Like above, but for skin files
#   CHROME_L10N_DIR - Like above, but for localization files
#   CHROME_TYPE - The type of chrome being generated (content, skin, locale).
#                  Top-level makefiles (the same one copying the rdf manifests
#                  and generating the jar file) should define this macro.
#                  This will notify the chrome registry of a new installation.

!if "$(CHROME_DIR)" != "$(NULL)"

# Figure out root of chrome dist dir.
CHROME_DIST=$(DIST)\bin\chrome\$(CHROME_DIR:/=\)

# Content
!if "$(CHROME_CONTENT)" != "$(NULL)"

CHROME_CONTENT=$(CHROME_CONTENT:/=\)

# Content goes to CHROME_DIR unless specified otherwise.
!if "$(CHROME_CONTENT_DIR)" == "$(NULL)"
CHROME_CONTENT_DIR=.
!endif

# Export content files by copying to dist.
chrome:: $(CHROME_CONTENT:.\=INSTALL\.\)

# Pseudo-target specifying how to install content files.
$(CHROME_CONTENT:.\=INSTALL\.\):
    $(MAKE_INSTALL) $(@:INSTALL\.=.) $(CHROME_DIST)\$(CHROME_CONTENT_DIR)

# Clobber content files.
clobber_all:: $(CHROME_CONTENT:.\=CLOBBER\.\)

# Pseudo-target specifying how to clobber content files.
$(CHROME_CONTENT:.\=CLOBBER\.\):
    -@$(RM) $(CHROME_DIST)\$(CHROME_CONTENT_DIR)\$(@:CLOBBER\.=.)

!endif # content

# Skin
!if "$(CHROME_SKIN)" != "$(NULL)"

CHROME_SKIN=$(CHROME_SKIN:/=\)

# Skin goes to CHROME_DIR unless specified otherwise.
!if "$(CHROME_SKIN_DIR)" == "$(NULL)"
CHROME_SKIN_DIR=.
!endif

# Export skin files by copying to dist.
chrome:: $(CHROME_SKIN:.\=INSTALL\.\)

# Pseudo-target specifying how to install chrome files.
$(CHROME_SKIN:.\=INSTALL\.\):
    $(MAKE_INSTALL) $(@:INSTALL\.=.) $(CHROME_DIST)\$(CHROME_SKIN_DIR)

# Clobber content files.
clobber_all:: $(CHROME_SKIN:.\=CLOBBER\.\)

# Pseudo-target specifying how to clobber content files.
$(CHROME_SKIN:.\=CLOBBER\.\):
    -@$(RM) $(CHROME_DIST)\$(CHROME_SKIN_DIR)\$(@:CLOBBER\.=.)

!endif # skin

# Localization.
!if "$(CHROME_L10N)" != "$(NULL)"

CHROME_L10N=$(CHROME_L10N:/=\)

# L10n goes to CHROME_DIR unless specified otherwise.
!if "$(CHROME_L10N_DIR)" == "$(NULL)"
CHROME_L10N_DIR=.
!endif

# Export l10n files by copying to dist.
chrome:: $(CHROME_L10N:.\=INSTALL\.\)

# Pseudo-target specifying how to install l10n files.
$(CHROME_L10N:.\=INSTALL\.\):
    $(MAKE_INSTALL) $(@:INSTALL\.=.) $(CHROME_DIST)\$(CHROME_L10N_DIR)

# Clobber l10n files.
clobber_all:: $(CHROME_L10N:.\=CLOBBER\.\)

# Pseudo-target specifying how to clobber l10n files.
$(CHROME_L10N:.\=CLOBBER\.\):
    -@$(RM) $(CHROME_DIST)\$(CHROME_L10N_DIR)\$(@:CLOBBER\.=.)

!endif # localization

# miscellaneous chrome
!if "$(CHROME_MISC)" != "$(NULL)"

CHROME_MISC=$(CHROME_MISC:/=\)

# misc goes to CHROME_DIR unless specified otherwise.
!if "$(CHROME_MISC_DIR)" == "$(NULL)"
CHROME_MISC_DIR=.
!endif

# Export misc files by copying to dist.
chrome:: $(CHROME_MISC:.\=INSTALL\.\)

# Pseudo-target specifying how to install misc files.
$(CHROME_MISC:.\=INSTALL\.\):
    $(MAKE_INSTALL) $(@:INSTALL\.=.) $(CHROME_DIST)\$(CHROME_MISC_DIR)

# Clobber misc files.
clobber_all:: $(CHROME_MISC:.\=CLOBBER\.\)

# Pseudo-target specifying how to clobber misc files.
$(CHROME_MISC:.\=CLOBBER\.\):
    -@$(RM) $(CHROME_DIST)\$(CHROME_MISC_DIR)\$(@:CLOBBER\.=.)

!endif # miscellaneous chrome

!ifdef MOZ_DISABLE_JAR_PACKAGING
!if "$(CHROME_TYPE)" != "$(NULL)"
chrome::
    -for %t in ($(CHROME_TYPE)) do echo %t,install,url,resource:/chrome/$(CHROME_DIR:\=/)/ >>$(DIST)\bin\chrome\installed-chrome.txt
!endif
!endif

chrome::
        @echo.
        @echo ****************** IF YOU ADD OR REMOVE CHROME FILES, PLEASE UPDATE $(MAKEDIR)\$(JAR_MANIFEST) (or tell warren@netscape.com) **********************
        @echo.

!endif # chrome

regchrome:
!if !defined(MOZ_DISABLE_JAR_PACKAGING)
  $(PERL) $(DEPTH)\config\zipchrome.pl win32 update $(DIST)\bin\chrome
  $(PERL) $(DEPTH)\config\installchrome.pl win32 jar $(DIST)\bin\chrome
!else
#  XXX do it this way once everyone has perl installed. For now some
#  people apparently have trouble with that so do the rules.mak stuff instead
#    $(PERL) $(DEPTH)\config\installchrome.pl win32 resource $(DIST)\bin\chrome
!endif

!endif # !MOZ_OLD_JAR_PACKAGING

################################################################################

# Easier than typing it by hand, works from any directory:
debug::
        start msdev $(DIST)\bin\mozilla.exe

# Easier than typing it by hand, works from any directory:
run::
        $(DIST)\bin\mozilla.exe

################################################################################
!endif # CONFIG_RULES_MAK
################################################################################
