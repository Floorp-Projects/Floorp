# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

#######################################################################
###                                                                 ###
###              R U L E S   O F   E N G A G E M E N T              ###
###                                                                 ###
#######################################################################

#######################################################################
# Double-Colon rules for utilizing the binary release model.          #
#######################################################################

DOCDIRS = nglayout xpcom img dom netlib raptor trex xpfc pref nls julian
DOCDIRS_PRIVATE = trex xpfc

all:: export libs program install

ifeq ($(AUTOCLEAN),1)
autobuild:: clean export libs program install
else
autobuild:: export libs program install
endif

platform::
	@echo $(OBJDIR_NAME)


#
# IMPORTS will always be associated with a component.  Therefore,
# the "import" rule will always change directory to the top-level
# of a component, and traverse the IMPORTS keyword from the
# "manifest.mn" file located at this level only.
#
# note: if there is a trailing slash, the component will be appended
#       (see import.pl - only used for xpheader.jar)

import::
	@echo "== import.pl =="
	@perl -I$(GDEPTH)/gconfig $(GDEPTH)/gconfig/import.pl \
		"RELEASE_TREE=$(RELEASE_TREE)"   \
		"IMPORTS=$(IMPORTS)"             \
		"VERSION=$(VERSION)" \
		"OS_ARCH=$(OS_ARCH)"             \
		"PLATFORM=$(PLATFORM)" \
		"OVERRIDE_IMPORT_CHECK=$(OVERRIDE_IMPORT_CHECK)"   \
		"ALLOW_VERSION_OVERRIDE=$(ALLOW_VERSION_OVERRIDE)" \
		"SOURCE_RELEASE_PREFIX=$(SOURCE_RELEASE_XP_DIR)"   \
		"SOURCE_MD_DIR=$(SOURCE_MD_DIR)"      \
		"SOURCE_XP_DIR=$(SOURCE_XP_DIR)"      \
		"FILES=$(XPCLASS_JAR) $(XPHEADER_JAR) $(MDHEADER_JAR) $(MDBINARY_JAR)" \
		"$(XPCLASS_JAR)=$(RELEASE_XP_DIR)|$(SOURCE_CLASSES_DIR)|"    \
		"$(XPHEADER_JAR)=$(RELEASE_XP_DIR)|$(SOURCE_XP_DIR)/public/|v" \
		"$(MDHEADER_JAR)=$(RELEASE_MD_DIR)|$(SOURCE_MD_DIR)/include|"        \
		"$(MDBINARY_JAR)=$(RELEASE_MD_DIR)|$(SOURCE_MD_DIR)|"

#	perl -I$(GDEPTH)/gconfig $(GDEPTH)/gconfig/import.pl \
#		"IMPORTS=$(IMPORTS)"             \
#		"RELEASE_TREE=$(RELEASE_TREE)"   \
#		"VERSION=$(VERSION)"             \
#		"PLATFORM=$(PLATFORM)"           \
#		"SOURCE_MD_DIR=$(SOURCE_MD_DIR)" \
#		"SOURCE_XP_DIR=$(SOURCE_XP_DIR)" \
#		"OS_ARCH=$(OS_ARCH)" ;


export::
	+$(LOOP_OVER_EXPORT_DIRS)

release_export::
	+$(LOOP_OVER_DIRS)

libs::
	+$(LOOP_OVER_LIBS_DIRS)

libs program install:: $(TARGETS)
ifdef LIBRARY
	$(INSTALL) -m 664 $(LIBRARY) $(SOURCE_LIB_DIR)
endif
ifdef SHARED_LIBRARY
	$(INSTALL) -m 775 $(SHARED_LIBRARY) $(SOURCE_BIN_DIR)
endif
ifdef IMPORT_LIBRARY
	$(INSTALL) -m 775 $(IMPORT_LIBRARY) $(SOURCE_LIB_DIR)
endif
ifdef PROGRAM
	$(INSTALL) -m 775 $(PROGRAM) $(SOURCE_BIN_DIR)
endif
ifdef PROGRAMS
	$(INSTALL) -m 775 $(PROGRAMS) $(SOURCE_BIN_DIR)
endif

tests::
	+$(LOOP_OVER_ALL_DIRS)

clean clobber::
	rm -rf $(ALL_TRASH)
	+$(LOOP_OVER_ALL_DIRS)

realclean clobber_all::
	rm -rf $(wildcard *.OBJ) dist $(ALL_TRASH)
	+$(LOOP_OVER_ALL_DIRS)

depend::
	+$(LOOP_OVER_LIBS_DIRS)

#ifdef ALL_PLATFORMS
#all_platforms:: $(NFSPWD)
#	@d=`$(NFSPWD)`;							\
#	if test ! -d LOGS; then rm -rf LOGS; mkdir LOGS; fi;		\
#	for h in $(PLATFORM_HOSTS); do					\
#		echo "On $$h: $(MAKE) $(ALL_PLATFORMS) >& LOGS/$$h.log";\
#		rsh $$h -n "(chdir $$d;					\
#			     $(MAKE) $(ALL_PLATFORMS) >& LOGS/$$h.log;	\
#			     echo DONE) &" 2>&1 > LOGS/$$h.pid &	\
#		sleep 1;						\
#	done
#
#$(NFSPWD):
#	cd $(@D); $(MAKE) $(@F)
#endif

#######################################################################
# Double-Colon rules for populating the binary release model.         #
#######################################################################
docs::
	rm -rf $(GDEPTH)/dist/docs
	@mkdir $(GDEPTH)/dist/docs
	@for d in $(DOCDIRS); do							\
		$(DOCXX) $(DOCXX_FLAGS) -d $(MOZ_SRC)\\mozilla\\dist\\docs\\$$d $(MOZ_SRC)\\mozilla\\dist\\public\\$$d\\*.h; \
	done
#	@for d in $(DOCDIRS_PRIVATE); do							\
#		$(DOCXX) $(DOCXX_FLAGS) -d $(MOZ_SRC)\\mozilla\\dist\\docs\\$$d $(MOZ_SRC)\\mozilla\\dist\\private\\$$d\\*.h; \
#	done
	@echo Documentation written to $(GDEPTH)/dist/docs


release_clean::
	rm -rf $(SOURCE_XP_DIR)/release

release:: release_clean release_export release_md release_jars release_cpdistdir

release_cpdistdir::
	@echo "== cpdist.pl =="
	@perl -I$(GDEPTH)/gconfig $(GDEPTH)/gconfig/cpdist.pl \
		"RELEASE_TREE=$(RELEASE_TREE)" \
		"GDEPTH=$(GDEPTH)" \
		"MODULE=${MODULE}" \
		"OS_ARCH=$(OS_ARCH)" \
		"RELEASE=$(RELEASE)" \
		"PLATFORM=$(PLATFORM)" \
		"RELEASE_VERSION=$(RELEASE_VERSION)" \
		"SOURCE_RELEASE_PREFIX=$(SOURCE_RELEASE_XP_DIR)" \
		"RELEASE_XP_DIR=$(RELEASE_XP_DIR)" \
		"RELEASE_MD_DIR=$(RELEASE_MD_DIR)" \
		"FILES=$(XPCLASS_JAR) $(XPHEADER_JAR) $(MDHEADER_JAR) $(MDBINARY_JAR) XP_FILES MD_FILES" \
		"$(XPCLASS_JAR)=$(SOURCE_RELEASE_CLASSES_DIR)|x"\
		"$(XPHEADER_JAR)=$(SOURCE_RELEASE_XPHEADERS_DIR)|x" \
		"$(MDHEADER_JAR)=$(SOURCE_RELEASE_MDHEADERS_DIR)|m" \
		"$(MDBINARY_JAR)=$(SOURCE_RELEASE_MD_DIR)|m" \
		"XP_FILES=$(XP_FILES)|xf" \
		"MD_FILES=$(MD_FILES)|mf"


# Add $(SOURCE_RELEASE_XPSOURCE_JAR) to FILES line when ready

# $(SOURCE_RELEASE_xxx_JAR) is a name like yyy.jar
# $(SOURCE_RELEASE_xx_DIR)  is a name like 

release_jars::
	@echo "== release.pl =="
	@perl -I$(GDEPTH)/gconfig $(GDEPTH)/gconfig/release.pl \
		"RELEASE_TREE=$(RELEASE_TREE)" \
		"PLATFORM=$(PLATFORM)" \
		"OS_ARCH=$(OS_ARCH)" \
		"RELEASE_VERSION=$(RELEASE_VERSION)" \
		"SOURCE_RELEASE_DIR=$(SOURCE_RELEASE_DIR)" \
		"FILES=$(XPCLASS_JAR) $(XPHEADER_JAR) $(MDHEADER_JAR) $(MDBINARY_JAR)" \
		"$(XPCLASS_JAR)=$(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_CLASSES_DIR)|b"\
		"$(XPHEADER_JAR)=$(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_XPHEADERS_DIR)|a" \
		"$(MDHEADER_JAR)=$(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_MDHEADERS_DIR)|a" \
		"$(MDBINARY_JAR)=$(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_MD_DIR)|bi"
#		"$(XPSOURCE_JAR)=$(SOURCE_RELEASE_PREFIX)|a"



release_md::
ifdef LIBRARY
	$(INSTALL) -m 444 $(LIBRARY) $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)
endif
ifdef SHARED_LIBRARY
	$(INSTALL) -m 555 $(SHARED_LIBRARY) $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)
endif
ifdef IMPORT_LIBRARY
	$(INSTALL) -m 555 $(IMPORT_LIBRARY) $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)
endif
ifdef PROGRAM
	$(INSTALL) -m 555 $(PROGRAM) $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_BIN_DIR)
endif
ifdef PROGRAMS
	$(INSTALL) -m 555 $(PROGRAMS) $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_BIN_DIR)
endif
	+$(LOOP_OVER_DIRS)


alltags:
	rm -f TAGS
	find . -name dist -prune -o \( -name '*.[hc]' -o -name '*.cp' -o -name '*.cpp' \) -print | xargs etags -a
	find . -name dist -prune -o \( -name '*.[hc]' -o -name '*.cp' -o -name '*.cpp' \) -print | xargs ctags -a

$(RESFILE):	
	echo Creating Resource file: $(RESFILE)
	$(RC) $(RCFLAGS) -r -Fo$(RESFILE) $(RCFILE)

$(PROGRAM): $(OBJS) $(RESFILE)
	@$(MAKE_OBJDIR)
ifeq ($(OS_ARCH),WINNT)
ifeq ($(OS_TARGET),WIN16)
	echo system windows >w16link
	echo option map >>w16link
	echo option oneautodata >>w16link
	echo option heapsize=32K >>w16link
	echo debug watcom all >>w16link
	echo name $@ >>w16link
	echo file >>w16link
	echo $(W16OBJS) , >>w16link
	echo $(W16LDFLAGS) >> w16link
	echo library >>w16link
	echo winsock.lib >>w16link
	$(LINK) @w16link.
	rm w16link
else
	$(LINK_EXE) $(MAP) $(PDB) $(IMP) $(OBJS) $(OUT_NAME)$(PROGRAM) $(LDFLAGS) $(STATIC_LIBS) $(OS_LIBS) $(LD_LIBS) $(EXTRA_LIBS) $(RESFILE)
endif
else
	$(LINK_PROGRAM) -o $@ $(CFLAGS) $(OBJS) $(LDFLAGS) $(STATIC_LIBS) $(LD_LIBS) $(EXTRA_LIBS) $(OS_LIBS)
endif

$(LIBRARY): $(OBJS) $(AR_LIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(AR) $(OBJS) $(AR_LIBS)
ifeq ($(OS_ARCH), WINNT)
ifdef AR_LIBS
	+$(EXTRACT_OBJS)
endif
endif
	$(RANLIB) $@

ifeq ($(OS_TARGET), WIN16)
$(IMPORT_LIBRARY): $(SHARED_LIBRARY)
	wlib +$(SHARED_LIBRARY)
endif

$(SHARED_LIBRARY): $(OBJS) $(AR_LIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	+$(AR_OBJS)
ifeq ($(OS_ARCH), AIX)
	echo "#!" > $(OBJDIR)/lib$(LIBRARY_NAME)_syms
	nm -B -C -g $(OBJS) \
	| awk '/ [T,D] / {print $$3}' \
	| sed -e 's/^\.//' \
	| sort -u >> $(OBJDIR)/lib$(LIBRARY_NAME)_syms
	$(LD) $(XCFLAGS) -o $@ $(OBJS) -bE:$(OBJDIR)/lib$(LIBRARY_NAME)_syms \
	-bM:SRE -bnoentry $(OS_LIBS) $(EXTRA_LIBS)
else
ifeq ($(OS_ARCH), WINNT)
ifeq ($(OS_TARGET), WIN16)
		echo system windows dll initinstance >w16link
		echo option map >>w16link
		echo option oneautodata >>w16link
		echo option heapsize=32K >>w16link
		echo debug watcom all >>w16link
		echo name $@ >>w16link
		echo file >>w16link
		echo $(W16OBJS) >>w16link
		echo $(W16LIBS) >>w16link
		echo libfile libentry >>w16link
		$(LINK) @w16link.
		rm w16link
else
ifdef AR_LIBS
		$(LINK_DLL) -MAP $(DLLBASE) $(AR_LIBS) $(OS_LIBS) $(LD_LIBS) $(EXTRA_LIBS) $(OBJS) $(LDFLAGS) @$(OBJDIR)\\$(LIBRARY_NAME).lst
else
		$(LINK_DLL) -MAP $(DLLBASE) $(AR_LIBS) $(OS_LIBS) $(LD_LIBS) $(EXTRA_LIBS) $(OBJS) $(LDFLAGS) 
endif
endif
else
	$(MKSHLIB) -o $@ $(OBJS) $(AR_ALL) $(AR_LIBS) $(AR_NONE) $(LD_LIBS) $(EXTRA_LIBS) $(OS_LIBS)
	chmod +x $@
endif
endif

$(PURE_LIBRARY):
	rm -f $@
ifneq ($(OS_ARCH), WINNT)
	$(AR) $(OBJS)
endif
	$(RANLIB) $@

ifeq ($(OS_ARCH), WINNT)
$(RES): $(RESNAME)
	@$(MAKE_OBJDIR)
	$(RC) -Fo$(RES) $(RESNAME)
	@echo $(RES) finished

$(DLL): $(OBJS) $(EXTRA_LIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(LINK_DLL) $(OBJS) $(OS_LIBS) $(EXTRA_LIBS)
endif

$(OBJDIR)/$(PROG_PREFIX)%$(PROG_SUFFIX): $(OBJDIR)/$(PROG_PREFIX)%$(OBJ_SUFFIX)
	@$(MAKE_OBJDIR)
ifeq ($(OS_ARCH),WINNT)
	$(CC) $(OBJDIR)/$(PROG_PREFIX)$*$(OBJ_SUFFIX) -Fe$@ -link $(LDFLAGS) $(OS_LIBS) $(EXTRA_LIBS)
else
	$(CC) -o $@ $(OBJDIR)/$(PROG_PREFIX)$*$(OBJ_SUFFIX) $(LDFLAGS)
endif

ifdef HAVE_PURIFY
$(OBJDIR)/$(PROG_PREFIX)%.pure: $(OBJDIR)/%$(OBJ_SUFFIX)
	@$(MAKE_OBJDIR)
ifeq ($(OS_ARCH),WINNT)
	$(PURIFY) $(CC) -Fo$@ -c $(CFLAGS) $(OBJDIR)/$(PROG_PREFIX)$*$(OBJ_SUFFIX) $(PURELDFLAGS)
else
	$(PURIFY) $(CC) -o $@ $(CFLAGS) $(OBJDIR)/$(PROG_PREFIX)$*$(OBJ_SUFFIX) $(PURELDFLAGS)
endif
endif

WCCFLAGS1 := $(subst /,\\,$(CFLAGS))
WCCFLAGS2 := $(subst -I,-i=,$(WCCFLAGS1))
WCCFLAGS3 := $(subst -D,-d,$(WCCFLAGS2))

$(OBJDIR)/$(PROG_PREFIX)%$(OBJ_SUFFIX): %.c
	@$(MAKE_OBJDIR)
ifeq ($(OS_ARCH), WINNT)
ifeq ($(OS_TARGET), WIN16)
		echo $(WCCFLAGS3) >w16wccf
		$(CC) -zq -fo$(OBJDIR)\\$(PROG_PREFIX)$*$(OBJ_SUFFIX)  @w16wccf $*.c
		rm w16wccf
else
		$(CC) -Fo$@ -c $(CFLAGS) $*.c
endif
else
	$(CC) -o $@ -c $(CFLAGS) $*.c
endif

$(OBJDIR)/$(PROG_PREFIX)%$(OBJ_SUFFIX): %.s
	@$(MAKE_OBJDIR)
	$(AS) -o $@ $(ASFLAGS) -c $*.s

$(OBJDIR)/$(PROG_PREFIX)%$(OBJ_SUFFIX): %.S
	@$(MAKE_OBJDIR)
	$(AS) -o $@ $(ASFLAGS) -c $*.S

$(OBJDIR)/$(PROG_PREFIX)%: %.cpp
	@$(MAKE_OBJDIR)
ifeq ($(OS_ARCH), WINNT)
	$(CCC) -Fo$@ -c $(CFLAGS) $<
else
	$(CCC) -o $@ -c $(CFLAGS) $<
endif

#
# Please keep the next two rules in sync.
#
$(OBJDIR)/$(PROG_PREFIX)%$(OBJ_SUFFIX): %.cc
	@$(MAKE_OBJDIR)
	$(CCC) -o $@ -c $(CFLAGS) $*.cc

$(OBJDIR)/$(PROG_PREFIX)%$(OBJ_SUFFIX): %.cpp
	@$(MAKE_OBJDIR)
ifdef STRICT_CPLUSPLUS_SUFFIX
	echo "#line 1 \"$*.cpp\"" | cat - $*.cpp > $(OBJDIR)/t_$*.cc
	$(CCC) -o $@ -c $(CFLAGS) $(OBJDIR)/t_$*.cc
	rm -f $(OBJDIR)/t_$*.cc
else
ifeq ($(OS_ARCH),WINNT)
	$(CCC) -Fo$@ -c $(CFLAGS) $*.cpp
else
	$(CCC) -o $@ -c $(CFLAGS) $*.cpp
endif
endif #STRICT_CPLUSPLUS_SUFFIX

%.i: %.cpp
ifeq ($(OS_TARGET), WIN16)
	echo $(WCCFLAGS3) >w16wccf
	$(CCC) -pl -fo=$* @w16wccf $*.cpp
	rm w16wccf
else
	$(CCC) -C -E $(CFLAGS) $< > $*.i
endif

%.i: %.c
ifeq ($(OS_TARGET), WIN16)
	echo $(WCCFLAGS3) >w16wccf
	$(CC) -pl -fo=$* @w16wccf $*.c
	rm w16wccf
else
	$(CC) -C -E $(CFLAGS) $< > $*.i
endif

%: %.pl
	rm -f $@; cp $*.pl $@; chmod +x $@

%: %.sh
	rm -f $@; cp $*.sh $@; chmod +x $@

ifdef DIRS
$(DIRS)::
	@if test -d $@; then				\
		set $(EXIT_ON_ERROR);			\
		echo "cd $@; $(MAKE)";			\
		cd $@; $(MAKE);				\
		set +e;					\
	else						\
		echo "Skipping non-directory $@...";	\
	fi;						\
	$(CLICK_STOPWATCH)
endif

################################################################################
# Bunch of things that extend the 'export' rule (in order):
################################################################################

$(JAVA_DESTPATH) $(JAVA_DESTPATH)/$(PACKAGE) $(JMCSRCDIR)::
	@if test ! -d $@; then	    \
		echo Creating $@;   \
		rm -rf $@;	    \
		$(NSINSTALL) -D $@; \
	fi

################################################################################
## IDL_GEN

ifneq ($(IDL_GEN),)

#export::
#	$(IDL2JAVA) $(IDL_GEN)

#all:: export

#clobber::
#	rm -f $(IDL_GEN:.idl=.class)	# XXX wrong!

endif

################################################################################
### JSRCS -- for compiling java files

ifneq ($(JSRCS),)
export:: $(JAVA_DESTPATH) $(JAVA_DESTPATH)/$(PACKAGE)
	@list=`perl $(NETLIBDEPTH)/gconfig/outofdate.pl $(PERLARG)	\
		    -d $(JAVA_DESTPATH)/$(PACKAGE) $(JSRCS)`;	\
	if test "$$list"x != "x"; then				\
	    echo $(JAVAC) $$list;				\
	    $(JAVAC) $$list;					\
	fi

all:: export

clobber::
	rm -f $(SOURCE_XP_DIR)/classes/$(PACKAGE)/*.class

endif

#
# JDIRS -- like JSRCS, except you can give a list of directories and it will
# compile all the out-of-date java files in those directories.
#
# NOTE: recursing through these can speed things up, but they also cause
# some builds to run out of memory
#
ifdef JDIRS
export:: $(JAVA_DESTPATH) $(JAVA_DESTPATH)/$(PACKAGE)
	@for d in $(JDIRS); do							\
		if test -d $$d; then						\
			set $(EXIT_ON_ERROR);					\
			files=`echo $$d/*.java`;				\
			list=`perl $(NETLIBDEPTH)/gconfig/outofdate.pl $(PERLARG)	\
				    -d $(JAVA_DESTPATH)/$(PACKAGE) $$files`;	\
			if test "$${list}x" != "x"; then			\
			    echo Building all java files in $$d;		\
			    echo $(JAVAC) $$list;				\
			    $(JAVAC) $$list;					\
			fi;							\
			set +e;							\
		else								\
			echo "Skipping non-directory $$d...";			\
		fi;								\
		$(CLICK_STOPWATCH);						\
	done
endif

#
# JDK_GEN -- for generating "old style" native methods 
#
# Generate JDK Headers and Stubs into the '_gen' and '_stubs' directory
#
ifneq ($(JDK_GEN),)
ifdef NSBUILDROOT
	INCLUDES += -I$(JDK_GEN_DIR) -I$(SOURCE_XP_DIR)
else
	INCLUDES += -I$(JDK_GEN_DIR)
endif
JDK_PACKAGE_CLASSES	:= $(JDK_GEN)
JDK_PATH_CLASSES	:= $(subst .,/,$(JDK_PACKAGE_CLASSES))
JDK_HEADER_CLASSFILES	:= $(patsubst %,$(JAVA_DESTPATH)/%.class,$(JDK_PATH_CLASSES))
JDK_STUB_CLASSFILES	:= $(patsubst %,$(JAVA_DESTPATH)/%.class,$(JDK_PATH_CLASSES))
JDK_HEADER_CFILES	:= $(patsubst %,$(JDK_GEN_DIR)/%.h,$(JDK_GEN))
JDK_STUB_CFILES		:= $(patsubst %,$(JDK_STUB_DIR)/%.c,$(JDK_GEN))

$(JDK_HEADER_CFILES): $(JDK_HEADER_CLASSFILES)
$(JDK_STUB_CFILES): $(JDK_STUB_CLASSFILES)

export::
	@echo Generating/Updating JDK headers 
	$(JAVAH) -d $(JDK_GEN_DIR) $(JDK_PACKAGE_CLASSES)
	@echo Generating/Updating JDK stubs
	$(JAVAH) -stubs -d $(JDK_STUB_DIR) $(JDK_PACKAGE_CLASSES)
ifndef NO_MAC_JAVA_SHIT
	@if test ! -d $(NETLIBDEPTH)/lib/mac/Java/; then						\
		echo "!!! You need to have a ns/lib/mac/Java directory checked out.";		\
		echo "!!! This allows us to automatically update generated files for the mac.";	\
		echo "!!! If you see any modified files there, please check them in.";		\
	fi
	@echo Generating/Updating JDK headers for the Mac
	$(JAVAH) -mac -d $(NETLIBDEPTH)/lib/mac/Java/_gen $(JDK_PACKAGE_CLASSES)
	@echo Generating/Updating JDK stubs for the Mac
	$(JAVAH) -mac -stubs -d $(NETLIBDEPTH)/lib/mac/Java/_stubs $(JDK_PACKAGE_CLASSES)
endif
endif

#
# JRI_GEN -- for generating JRI native methods
#
# Generate JRI Headers and Stubs into the 'jri' directory
#
ifneq ($(JRI_GEN),)
ifdef NSBUILDROOT
	INCLUDES += -I$(JRI_GEN_DIR) -I$(SOURCE_XP_DIR)
else
	INCLUDES += -I$(JRI_GEN_DIR)
endif
JRI_PACKAGE_CLASSES	:= $(JRI_GEN)
JRI_PATH_CLASSES	:= $(subst .,/,$(JRI_PACKAGE_CLASSES))
JRI_HEADER_CLASSFILES	:= $(patsubst %,$(JAVA_DESTPATH)/%.class,$(JRI_PATH_CLASSES))
JRI_STUB_CLASSFILES	:= $(patsubst %,$(JAVA_DESTPATH)/%.class,$(JRI_PATH_CLASSES))
JRI_HEADER_CFILES	:= $(patsubst %,$(JRI_GEN_DIR)/%.h,$(JRI_GEN))
JRI_STUB_CFILES		:= $(patsubst %,$(JRI_GEN_DIR)/%.c,$(JRI_GEN))

$(JRI_HEADER_CFILES): $(JRI_HEADER_CLASSFILES)
$(JRI_STUB_CFILES): $(JRI_STUB_CLASSFILES)

export::
	@echo Generating/Updating JRI headers 
	$(JAVAH) -jri -d $(JRI_GEN_DIR) $(JRI_PACKAGE_CLASSES)
	@echo Generating/Updating JRI stubs
	$(JAVAH) -jri -stubs -d $(JRI_GEN_DIR) $(JRI_PACKAGE_CLASSES)
ifndef NO_MAC_JAVA_SHIT
	@if test ! -d $(NETLIBDEPTH)/lib/mac/Java/; then						\
		echo "!!! You need to have a ns/lib/mac/Java directory checked out.";		\
		echo "!!! This allows us to automatically update generated files for the mac.";	\
		echo "!!! If you see any modified files there, please check them in.";		\
	fi
	@echo Generating/Updating JRI headers for the Mac
	$(JAVAH) -jri -mac -d $(NETLIBDEPTH)/lib/mac/Java/_jri $(JRI_PACKAGE_CLASSES)
	@echo Generating/Updating JRI stubs for the Mac
	$(JAVAH) -jri -mac -stubs -d $(NETLIBDEPTH)/lib/mac/Java/_jri $(JRI_PACKAGE_CLASSES)
endif
endif

#
# JMC_EXPORT -- for declaring which java classes are to be exported for jmc
#
ifneq ($(JMC_EXPORT),)
JMC_EXPORT_PATHS	:= $(subst .,/,$(JMC_EXPORT))
JMC_EXPORT_FILES	:= $(patsubst %,$(JAVA_DESTPATH)/$(PACKAGE)/%.class,$(JMC_EXPORT_PATHS))

#
# We're doing NSINSTALL -t here (copy mode) because calling INSTALL will pick up 
# your NSDISTMODE and make links relative to the current directory. This is a
# problem because the source isn't in the current directory:
#
export:: $(JMC_EXPORT_FILES) $(JMCSRCDIR)
	$(NSINSTALL) -t -m 444 $(JMC_EXPORT_FILES) $(JMCSRCDIR)
endif

#
# JMC_GEN -- for generating java modules
#
# Provide default export & install rules when using JMC_GEN
#
ifneq ($(JMC_GEN),)
	INCLUDES    += -I$(JMC_GEN_DIR) -I.
	JMC_HEADERS := $(patsubst %,$(JMC_GEN_DIR)/%.h,$(JMC_GEN))
	JMC_STUBS   := $(patsubst %,$(JMC_GEN_DIR)/%.c,$(JMC_GEN))
	JMC_OBJS    := $(patsubst %,$(OBJDIR)/%$(OBJ_SUFFIX),$(JMC_GEN))

$(JMC_GEN_DIR)/M%.h: $(JMCSRCDIR)/%.class
	$(JMC) -d $(JMC_GEN_DIR) -interface $(JMC_GEN_FLAGS) $(?F:.class=)

$(JMC_GEN_DIR)/M%.c: $(JMCSRCDIR)/%.class
	$(JMC) -d $(JMC_GEN_DIR) -module $(JMC_GEN_FLAGS) $(?F:.class=)

$(OBJDIR)/M%$(OBJ_SUFFIX): $(JMC_GEN_DIR)/M%.h $(JMC_GEN_DIR)/M%.c
	@$(MAKE_OBJDIR)
	$(CC) -o $@ -c $(CFLAGS) $(JMC_GEN_DIR)/M$*.c

export:: $(JMC_HEADERS) $(JMC_STUBS)
endif

#
# Copy each element of EXPORTS to $(SOURCE_XP_DIR)/public/$(MODULE)/
#
PUBLIC_EXPORT_DIR = $(SOURCE_XP_DIR)/public/$(MODULE)
ifeq ($(OS_ARCH),WINNT)
ifeq ($(OS_TARGET),WIN16)
PUBLIC_EXPORT_DIR = $(SOURCE_XP_DIR)/public/win16
endif
endif

ifneq ($(EXPORTS),)
$(PUBLIC_EXPORT_DIR)::
	@if test ! -d $@; then	    \
		echo Creating $@;   \
		$(NSINSTALL) -D $@; \
	fi

export:: $(EXPORTS) $(PUBLIC_EXPORT_DIR)
	$(INSTALL) -m 444 $(EXPORTS) $(PUBLIC_EXPORT_DIR)
endif

# Duplicate export rule for private exports, with different directories

PRIVATE_EXPORT_DIR = $(SOURCE_XP_DIR)/private/$(MODULE)
ifeq ($(OS_ARCH),WINNT)
ifeq ($(OS_TARGET),WIN16)
PRIVATE_EXPORT_DIR = $(SOURCE_XP_DIR)/public/win16
endif
endif

ifneq ($(PRIVATE_EXPORTS),)
$(PRIVATE_EXPORT_DIR)::
	@if test ! -d $@; then	    \
		echo Creating $@;   \
		$(NSINSTALL) -D $@; \
	fi

export:: $(PRIVATE_EXPORTS) $(PRIVATE_EXPORT_DIR)
	$(INSTALL) -m 444 $(PRIVATE_EXPORTS) $(PRIVATE_EXPORT_DIR)
else
export:: 
	@echo There are no private exports.;
endif

# Duplicate export rule for tests, with different directories


TESTS_DIR = $(RESULTS_DIR)/$(RESULTS_SUBDIR)/$(OS_CONFIG)$(CPU_TAG)$(COMPILER_TAG)$(IMPL_STRATEGY)

ifneq ($(REGRESSION_SPEC),)
ifneq ($(OS_ARCH),WINNT)
tests:: $(REGRESSION_SPEC) $(GDEPTH)/$(MODULE)/scripts/regress.pl $(SOURCE_MD_DIR)/bin/reporter$(PROG_SUFFIX)
	cd $(PLATFORM); \
	perl5 -I ../$(GDEPTH)/$(MODULE)/scripts \
              ../$(GDEPTH)/$(MODULE)/scripts/regress.pl \
              specfile=../$(REGRESSION_SPEC) platform=$(PLATFORM) progress; \
	if test ! -d $(TESTS_DIR); then	\
		echo Creating $(TESTS_DIR);   \
		../$(NSINSTALL) -D $(TESTS_DIR); \
	fi
ifneq ($(BUILD_OPT),)
	cd $(PLATFORM); \
	../$(NSINSTALL) -m 664 $(REGDATE).sum $(TESTS_DIR); \
	../$(NSINSTALL) -m 664 $(REGDATE).htm $(TESTS_DIR); \
	../$(SOURCE_MD_DIR)/bin/reporter$(PROG_SUFFIX) rptdir=$(RELEASE_TREE)/$(MODULE)/results specfile=$(RESULTS_DIR)/rptspec 
endif
else
tests:: $(REGRESSION_SPEC) $(SOURCE_MD_DIR)/bin/regress$(PROG_SUFFIX) $(SOURCE_MD_DIR)/bin/reporter$(PROG_SUFFIX)
	cd $(PLATFORM); \
	../$(SOURCE_MD_DIR)/bin/regress$(PROG_SUFFIX) specfile=../$(REGRESSION_SPEC) progress; \
	if test ! -d $(TESTS_DIR); then	\
		echo Creating $(TESTS_DIR);   \
		$(NSINSTALL) -D $(TESTS_DIR); \
	fi
ifneq ($(BUILD_OPT),)
	cd $(PLATFORM); \
	$(NSINSTALL) -m 664 $(REGDATE).sum $(TESTS_DIR); \
	$(NSINSTALL) -m 664 $(REGDATE).htm $(TESTS_DIR); \
	echo "Please now make sure your results files are copied to $(TESTS_DIR), "; \
	echo "then run 'reporter specfile=$(RESULTS_DIR)/rptspec'"
endif
endif
else
tests:: 
	@echo There are no regression suites. Define REGRESSION_SPEC in your manifest.mn file!;
endif

# Duplicate export rule for releases, with different directories

ifneq ($(EXPORTS),)
$(SOURCE_XP_DIR)/release/include::
	@if test ! -d $@; then	    \
		echo Creating $@;   \
		$(NSINSTALL) -D $@; \
	fi

release_export:: $(EXPORTS) $(SOURCE_XP_DIR)/release/include
	$(INSTALL) -m 444 $(EXPORTS) $(SOURCE_XP_DIR)/release/include
endif




################################################################################

-include $(DEPENDENCIES)

ifneq ($(OS_ARCH),WINNT)
# Can't use sed because of its 4000-char line length limit, so resort to perl
.DEFAULT:
	@perl -e '                                                            \
	    open(MD, "< $(DEPENDENCIES)");                                    \
	    while (<MD>) {                                                    \
		if (m@ \.*/*$< @) {                                           \
		    $$found = 1;                                              \
		    last;                                                     \
		}                                                             \
	    }                                                                 \
	    if ($$found) {                                                    \
		print "Removing stale dependency $< from $(DEPENDENCIES)\n";  \
		seek(MD, 0, 0);                                               \
		$$tmpname = "$(OBJDIR)/fix.md" . $$$$;                        \
		open(TMD, "> " . $$tmpname);                                  \
		while (<MD>) {                                                \
		    s@ \.*/*$< @ @;                                           \
		    if (!print TMD "$$_") {                                   \
			unlink(($$tmpname));                                  \
			exit(1);                                              \
		    }                                                         \
		}                                                             \
		close(TMD);                                                   \
		if (!rename($$tmpname, "$(DEPENDENCIES)")) {                  \
		    unlink(($$tmpname));                                      \
		}                                                             \
	    } elsif ("$<" ne "$(DEPENDENCIES)") {                             \
		print "$(MAKE): *** No rule to make target $<.  Stop.\n";     \
		exit(1);                                                      \
	    }'
endif

#############################################################################
# X dependency system
#############################################################################

ifneq ($(OS_ARCH),WINNT)

$(MKDEPENDENCIES)::
	@$(MAKE_OBJDIR)
	touch $(MKDEPENDENCIES)
	$(MKDEPEND) -p$(OBJDIR_NAME)/ -o'$(OBJ_SUFFIX)' -f$(MKDEPENDENCIES) $(INCLUDES) $(CSRCS) $(CPPSRCS) $(ASFILES)

$(MKDEPEND)::
	cd $(MKDEPEND_DIR); $(MAKE)

ifdef OBJS
depend:: $(MKDEPEND) $(MKDEPENDENCIES)
else
depend::
endif
	+$(LOOP_OVER_DIRS)

dependclean::
	rm -f $(MKDEPENDENCIES)
	+$(LOOP_OVER_DIRS)

-include $(NSINSTALL_DIR)/$(OBJDIR)/depend.mk

else
depend::
	@$(MAKE_OBJDIR)
	$(GDEPTH)/gconfig/makedep.exe -s -O $(OBJDIR)/make.dp $(INCLUDES) $(OBJS)
#	@touch $(OBJDIR)/make.dp
#	$(GDEPTH)/gconfig/mkdepend/$(OBJDIR)/mkdepend.exe -p$(OBJDIR) -o'.obj' -f$(OBJDIR)/make.dp $(INCLUDES) $(OBJS)
endif

################################################################################
# Special gmake rules.
################################################################################

#
# Re-define the list of default suffixes, so gmake won't have to churn through
# hundreds of built-in suffix rules for stuff we don't need.
#
.SUFFIXES:
.SUFFIXES: .out .a .ln .o .obj .c .cc .C .cpp .y .l .s .S .h .sh .i .pl .class .java .html

#
# Don't delete these files if we get killed.
#
.PRECIOUS: .java $(JDK_HEADERS) $(JDK_STUBS) $(JRI_HEADERS) $(JRI_STUBS) $(JMC_HEADERS) $(JMC_STUBS)

#
# Fake targets.  Always run these rules, even if a file/directory with that
# name already exists.
#
.PHONY: all all_platforms alltags boot clean clobber clobber_all export install libs realclean release $(OBJDIR) $(DIRS)

ifeq ($(OS_ARCH),WINNT)
-include $(OBJDIR)/make.dp
endif



