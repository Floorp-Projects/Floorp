# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.

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


MANIFEST_LEVEL=MACROS
!IF EXIST(manifest.mn) && !defined(IGNORE_MANIFEST)
!IF "$(WINOS)" == "WIN95"
!IF [$(DEPTH)\config\mantomak.exe manifest.mn manifest.mnw] == 0 
!INCLUDE <manifest.mnw>
!ELSE
!ERROR ERROR:  Unable to generate manifest.mnw from manifest.mn
!ENDIF
!ELSE
!IF ["$(DEPTH)\config\mantomak.exe manifest.mn manifest.mnw"] == 0 
!INCLUDE <manifest.mnw>
!ELSE
!ERROR ERROR:  Unable to generate manifest.mnw from manifest.mn
!ENDIF
!ENDIF
!ENDIF

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
    $(NMAKE) -f makefile.win libs
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
PDBFILE=.\$(OBJDIR)\default
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

$(XPDIST)\public\$(MODULE):
    -mkdir $(XPDIST:/=\)\public\$(MODULE:/=\)

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

!ifdef JDIRS
!if defined(JAVA_OR_NSJVM)
#//------------------------------------------------------------------------
#//
#// Rule to recursively make all subdirectories specified by the JDIRS target
#//
#//------------------------------------------------------------------------

export:: $(JAVA_DESTPATH) $(JDIRS)

$(JDIRS):: $(JAVA_DESTPATH) $(TMPDIR)

!if "$(WINOS)" == "WIN95"
JDIRS = $(JDIRS:/=\)
!endif

!if defined(NO_CAFE)

$(JDIRS)::
    @echo +++ make: building package: $@
    @echo $(JAVAC_PROG) $(JAVAC_FLAGS) > $(TMPDIR)\javac.cfg
    -@$(DEPTH)\config\buildpkg $(TMPDIR)\javac.cfg $@ 
    @$(RM) $(TMPDIR)\javac.cfg
#    @$(DEPTH)\config\buildpkg $@ $(DEPTH)\dist\classes

!else

# compile using symantec cafe's super-speedy compiler!
$(JDIRS)::
    @echo +++ make: building package $@
!if "$(WINOS)" == "WIN95"
    -@$(MKDIR) $(DEPTH)\dist\classes\$(@:/=\)
!else
    -@$(MKDIR) $(DEPTH)\dist\classes\$@ 2> NUL
!endif
    $(MOZ_TOOLS)\bin\sj -classpath $(JAVA_DESTPATH);$(JAVA_SOURCEPATH) \
            -d $(JAVA_DESTPATH) $(JAVAC_OPTIMIZER) $@\*.java


!endif # NO_CAFE

clobber::
    -for %g in ($(JDIRS)) do $(RM_R) $(XPDIST:/=\)/classes/%g

!endif # JAVA_OR_NSJVM
!endif # JDIRS

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

!ifdef LIBRARY_NAME
LIBRARY=$(OBJDIR)\$(LIBRARY_NAME)$(LIBRARY_SUFFIX).lib
!endif


#//------------------------------------------------------------------------
#//
#// Global rules...
#//
#//------------------------------------------------------------------------

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
    -$(RM_R) $(GARBAGE) $(OBJDIR) 2> NUL

clobber_all::
!ifdef DIRS
     @$(W95MAKE) clobber_all $(MAKEDIR) $(DIRS)
!endif
    -$(RM_R) *.OBJ $(TARGETS) $(GARBAGE) $(OBJDIR) 2> NUL

  
export::
!ifdef DIRS
    @$(W95MAKE) export $(MAKEDIR) $(DIRS)
!endif # DIRS

libs:: w95libs $(LIBRARY)

w95libs::
!ifdef DIRS
     @$(W95MAKE) libs $(MAKEDIR) $(DIRS)
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
#//------------------------------------------------------------------------

MAKEDEP=$(MOZ_SRC)\mozilla\config\makedep.exe
MAKEDEPFILE=.\$(OBJDIR:/=\)\make.dep

MAKEDEPDETECT=$(OBJS)
MAKEDEPDETECT=$(MAKEDEPDETECT: =)
MAKEDEPDETECT=$(MAKEDEPDETECT:	=)

!if !defined(NODEPEND) && "$(MAKEDEPDETECT)" != ""

depend:: $(OBJDIR)
    @echo Analyzing dependencies...
    $(MAKEDEP) -s -o $(MAKEDEPFILE) @<<
$(LINCS)
$(OBJS)
<<

!endif

!IF EXIST($(MAKEDEPFILE)) 
!INCLUDE <$(MAKEDEPFILE)>
!ENDIF

export:: $(DIRS)

libs:: $(DIRS) $(LIBRARY)

install:: $(DIRS)

depend:: $(DIRS)

mangle:: $(DIRS)
    $(MAKE_MANGLE)

unmangle:: $(DIRS)
    -$(MAKE_UNMANGLE)


alltags::
        echo Making emacs tags
	c:\\mksnt\\find . -name dist -prune -o ( -name '*.[hc]' -o -name '*.cpp' ) -print | c:\\mksnt\\xargs etags -a


#//------------------------------------------------------------------------
#//
#// Rule to create the object directory (if necessary)
#//
#//------------------------------------------------------------------------
$(OBJDIR):
	@echo +++ make: Creating directory: $(OBJDIR)
	echo.
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
#// JMC
#//
#// JSRCS   .java files to be compiled (.java extension included)
#//
#//------------------------------------------------------------------------
!if defined(JAVA_OR_NSJVM)
!if defined(JSRCS)

JSRCS_DEPS = $(JAVA_DESTPATH) $(JAVA_DESTPATH)\$(PACKAGE) $(TMPDIR)

# Can't get moz cafe to compile a single file
!if defined(NO_CAFE)

export:: $(JSRCS_DEPS)
    @echo +++ make: building package: $(PACKAGE)
	$(PERL) $(DEPTH)\config\outofdate.pl \
	-d $(JAVA_DESTPATH)\$(PACKAGE) $(JSRCS) >> $(TMPDIR)\javac.cfg
	-$(JAVAC_PROG) -argfile $(TMPDIR)\javac.cfg
	@$(RM) $(TMPDIR)\javac.cfg
!else

# compile using symantec cafe's super-speedy compiler!
export:: $(JSRC_DEPS)
    @echo +++ make: building package: $(PACKAGE)	
	@echo -d $(JAVA_DESTPATH) $(JAVAC_OPTIMIZER) \
	   -classpath $(JAVA_DESTPATH);$(JAVA_SOURCEPATH) > $(TMPDIR)\javac.cfg
	@$(PERL) $(DEPTH)\config\sj.pl \
	 $(JAVA_DESTPATH)\$(PACKAGE)\ $(TMPDIR)\javac.cfg <<
	$(JSRCS)
<<

!endif #NO_CAFE

clobber::
    -for %g in ($(JSRCS:.java=.class)) do $(RM) $(XPDIST:/=\)/classes/$(PACKAGE:/=\)/%g

!endif # JSRCS

#//------------------------------------------------------------------------
#//
#// JMC
#//
#// JMC_EXPORT  .class files to be copied from XPDIST/classes/PACKAGE to
#//                 XPDIST/jmc (without the .class extension)
#//
#//------------------------------------------------------------------------
!if defined(JMC_EXPORT)
export:: $(JMCSRCDIR)
    for %g in ($(JMC_EXPORT)) do $(MAKE_INSTALL:/=\) $(JAVA_DESTPATH)\$(PACKAGE:/=\)\%g.class $(JMCSRCDIR)

clobber::
    -for %f in ($(JMC_EXPORT)) do $(RM) $(JMCSRCDIR:/=\)\%f.class
!endif # JMC_EXPORT
!endif # JAVA_OR_NSJVM

#//------------------------------------------------------------------------
#//
#// JMC
#//
#// JMC_GEN Names of classes to be run through JMC
#//         Generated .h and .c files go to JMC_GEN_DIR
#//
#//------------------------------------------------------------------------
!if defined(JAVA_OR_NSJVM)

!if defined(JMC_GEN)
export:: $(JMC_HEADERS)

# Don't delete them if they don't compile (makes it hard to debug)
.PRECIOUS: $(JMC_HEADERS) $(JMC_STUBS)


# They may want to generate/compile the stubs
!if defined(CCJMC)
{$(JMC_GEN_DIR)\}.c{$(OBJDIR)\}.obj:
    @$(CC) @<<$(CFGFILE)
	-c $(CFLAGS)
	-I. -I$(JMC_GEN_DIR)
	-Fd$(PDBFILE)
	-Fo.\$(OBJDIR)\
	$(JMC_GEN_DIR)\$(*B).c
<<KEEP

export:: $(JMC_STUBS) $(OBJDIR) $(JMC_OBJS) 

!endif # CCJMC
!endif # JMC_GEN
!endif # JAVA_OR_NSJVM


#//------------------------------------------------------------------------
#//
#// JMC
#//
#// EXPORTS Names of headers to be copied to MODULE
#//
#//------------------------------------------------------------------------
!if "$(EXPORTS)" != "$(NULL)"
export:: $(XPDIST)\public\$(MODULE)
    for %f in ($(EXPORTS)) do $(MAKE_INSTALL:/=\) %f $(XPDIST:/=\)\public\$(MODULE:/=\)

clobber::
    -for %g in ($(EXPORTS)) do $(RM) $(XPDIST:/=\)\public\$(MODULE:/=\)\%g
!endif # EXPORTS

#//------------------------------------------------------------------------
#//  These rules must follow all lines that define the macros they use
#//------------------------------------------------------------------------
!if defined(JAVA_OR_NSJVM)
GARBAGE	= $(JMC_GEN_DIR) $(JMC_HEADERS) $(JMC_STUBS) \
	  $(JDK_STUB_DIR) $(JRI_GEN_DIR) $(JDK_GEN_DIR) $(JNI_GEN_DIR) 
!endif


clean:: $(DIRS)
    -$(RM) $(OBJS) $(NOSUCHFILE) NUL 2> NUL

clobber:: $(DIRS)
    -$(RM_R) $(GARBAGE) $(OBJDIR) 2> NUL

clobber_all:: $(DIRS)
    -$(RM_R) *.OBJ $(TARGETS) $(GARBAGE) $(OBJDIR) 2> NUL

MANIFEST_LEVEL=RULES
!IF EXIST(manifest.mnw) && !defined(IGNORE_MANIFEST)
!INCLUDE <manifest.mnw>
!ENDIF

!if "$(MOZ_BITS)"=="32"
CFLAGS = $(CFLAGS) -DNO_JNI_STUBS
!endif

NSINSTALL = nsinstall

!if "$(XPIDLSRCS)" != "$(NULL)"
!if "$(MODULE)" != "$(NULL)"
$(XPIDL_GEN_DIR):
	@echo +++ make: Creating directory: $(XPIDL_GEN_DIR)
	echo.
	-mkdir $(XPIDL_GEN_DIR)

$(DIST)\include:
	@echo +++ make: Creating directory: $(DIST)\include
	echo.
	-mkdir $(DIST)\include

$(XPIDL_GEN_MAKEFILE) : makefile.win 
	@if exist $(XPIDL_GEN_MAKEFILE) $(NMAKE) -f $(XPIDL_GEN_MAKEFILE) clobber
	@echo Generating $@
	@perl $(DEPTH)\config\xpidlmk.pl WINNT $(DEPTH) . $(XPIDL_GEN_DIR) $(DEPTH)\dist $(DIST)\bin\components $(MODULE) $(XPIDLSRCS) > $@

export:: $(XPIDL_GEN_DIR) $(DIST)\include $(XPIDL_GEN_MAKEFILE)
	@echo generating and copying xpidl based files as necessary
	@$(NMAKE) -f $(XPIDL_GEN_MAKEFILE) doxpidl

clobber::
	@echo deleting files generated from xpidl files
	@if exist $(XPIDL_GEN_MAKEFILE) $(NMAKE) -f $(XPIDL_GEN_MAKEFILE) clobber
	@$(RM_R) $(XPIDL_GEN_DIR)
!endif
!endif


!endif # CONFIG_RULES_MAK

