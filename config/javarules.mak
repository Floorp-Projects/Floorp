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

# This file contains make rules for building java files using mozilla's
# make system.  To use this file, you must include this file before
# including rules.mak. Like this:

# include <$(DEPTH)\config\javarules.mak>
# include <$(DEPTH)\config\rules.mak>


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
    @$(DEPTH)\config\buildpkg $@ $(DEPTH)\dist\classes

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

clobber clobber_all::
    -for %g in ($(JDIRS)) do $(RM_R) $(XPDIST:/=\)/classes/%g

!endif # JAVA_OR_NSJVM
!endif # JDIRS



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
	@echo $(TMPDIR)
#	@$(RM) $(TMPDIR)\javac.cfg
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
 
