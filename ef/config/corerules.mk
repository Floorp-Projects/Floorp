#
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

#######################################################################
###                                                                 ###
###              R U L E S   O F   E N G A G E M E N T              ###
###                                                                 ###
#######################################################################

#######################################################################
# Double-Colon rules for utilizing the binary release model.          #
#######################################################################

all:: export libs program install

autobuild:: export private_export libs program install

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
	@perl -I$(DEPTH)/config/core $(DEPTH)/config/core/import.pl \
		"RELEASE_TREE=$(RELEASE_TREE)"   \
		"IMPORTS=$(IMPORTS)"             \
		"VERSION=$(VERSION)"		 \
		"OS_ARCH=$(OS_ARCH)"             \
		"PLATFORM=$(PLATFORM)"		 \
		"SOURCE_RELEASE_PREFIX=$(SOURCE_RELEASE_XP_DIR)" \
		"SOURCE_MD_DIR=$(SOURCE_MD_DIR)"      \
		"SOURCE_XP_DIR=$(XPDIST)"      \
		"FILES=$(XPCLASS_JAR) $(XPHEADER_JAR) $(MDHEADER_JAR) $(MDBINARY_JAR)" \
		"$(XPCLASS_JAR)=$(RELEASE_XP_DIR)|$(SOURCE_CLASSES_DIR)"    \
		"$(XPHEADER_JAR)=$(RELEASE_XP_DIR)|$(XPDIST)/public/" \
		"$(MDHEADER_JAR)=$(RELEASE_MD_DIR)|$(SOURCE_MD_DIR)/include"        \
		"$(MDBINARY_JAR)=$(RELEASE_MD_DIR)|$(SOURCE_MD_DIR)"

#	perl -I$(DEPTH)/config/core $(DEPTH)/config/core/import.pl \
#		"IMPORTS=$(IMPORTS)"             \
#		"RELEASE_TREE=$(RELEASE_TREE)"   \
#		"VERSION=$(VERSION)"             \
#		"PLATFORM=$(PLATFORM)"           \
#		"SOURCE_MD_DIR=$(SOURCE_MD_DIR)" \
#		"SOURCE_XP_DIR=$(XPDIST)" \
#		"OS_ARCH=$(OS_ARCH)" ;


export::
ifndef NO_EXPORT_IN_SUBDIRS
	+$(LOOP_OVER_DIRS)
endif

private_export::
	+$(LOOP_OVER_DIRS)

release_export::
	+$(LOOP_OVER_DIRS)

libs :: $(TARGETS)
ifndef NO_LIBS_IN_SUBDIRS
	+$(LOOP_OVER_DIRS)
endif

program :: $(TARGETS)
ifndef NO_PROGRAM_IN_SUBDIRS
	+$(LOOP_OVER_DIRS)
endif

install:: $(TARGETS)
ifdef LIBRARY
	$(INSTALL) -m 444 $(LIBRARY) $(DIST)/lib
endif
ifdef SHARED_LIBRARY
	$(INSTALL) -m 555 $(SHARED_LIBRARY) $(DIST)/bin
endif
ifdef IMPORT_LIBRARY
	$(INSTALL) -m 555 $(IMPORT_LIBRARY) $(DIST)/lib
endif
ifdef PROGRAM
	$(INSTALL) -m 555 $(PROGRAM) $(DIST)/bin
endif
ifdef PROGRAMS
	$(INSTALL) -m 555 $(PROGRAMS) $(DIST)/bin
endif
ifndef NO_INSTALL_IN_SUBDIRS
	+$(LOOP_OVER_DIRS)
endif

tests::
	+$(LOOP_OVER_DIRS)

clean clobber::
	rm -rf $(ALL_TRASH)
	+$(LOOP_OVER_DIRS)

realclean clobber_all::
	rm -rf $(wildcard *.OBJ) dist $(ALL_TRASH)
	+$(LOOP_OVER_DIRS)

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


release_clean::
	rm -rf $(XPDIST)/release

release:: release_clean release_export release_md release_jars release_cpdistdir

release_cpdistdir::
	@echo "== cpdist.pl =="
	@perl -I$(DEPTH)/config/core $(DEPTH)/config/core/cpdist.pl \
		"RELEASE_TREE=$(RELEASE_TREE)" \
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
	@perl -I$(DEPTH)/config/core $(DEPTH)/config/core/release.pl \
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

$(PROGRAM): $(OBJS)
	@$(MAKE_OBJDIR)
ifeq ($(OS_ARCH),WINNT)
	$(CC) $(OBJS) -Fe$@ -link $(LDFLAGS) $(OS_LIBS) $(EXTRA_LIBS)
else
	$(CC) -o $@ $(CFLAGS) $(OBJS) $(LDFLAGS) $(OS_LIBS) 
endif
	$(INSTALL) -m 555 $(PROGRAM) $(DIST)/bin

$(LIBRARY): $(OBJS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(AR) $(OBJS)
	$(RANLIB) $@
	$(INSTALL) -m 444 $(LIBRARY) $(DIST)/lib


ifeq ($(OS_TARGET), WIN16)
$(IMPORT_LIBRARY): $(SHARED_LIBRARY)
	wlib +$(SHARED_LIBRARY)
	$(INSTALL) -m 555 $(IMPORT_LIBRARY) $(DIST)lib
endif

$(SHARED_LIBRARY): $(OBJS)
	@$(MAKE_OBJDIR)
	rm -f $@
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
		#echo extra: $(EXTRA_LIBS), os_libs: $(OS_LIBS), $(W16LIBS) >>w16link
		echo libfile libentry >>w16link
		$(LINK) @w16link.
		rm w16link
else
		$(LINK_DLL) -MAP $(DLLBASE) $(OS_LIBS) $(EXTRA_LIBS) $(OBJS) $(LDFLAGS)
endif
	$(INSTALL) -m 555 $(LIBRARY) $(DIST)/lib
else
	$(MKSHLIB) -o $@ $(OBJS) $(LD_LIBS) $(OS_LIBS) $(EXTRA_LIBS)
	chmod +x $@
endif
endif
ifeq ($(OS_ARCH),WINNT)
	$(INSTALL) -m 555 $(SHARED_LIBRARY) $(DIST)/bin
else
	$(INSTALL) -m 555 $(SHARED_LIBRARY) $(DIST)/lib
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
		#	$(DEPTH)/config/core/w16opt $(WCCFLAGS3)
		echo $(WCCFLAGS3) >w16wccf
		$(CC) -zq -fo$(OBJDIR)\\$(PROG_PREFIX)$*$(OBJ_SUFFIX)  @w16wccf $*.c
		rm w16wccf
else
ifdef GENERATE_BROWSE_INFO
		@mkdir $(BROWSE_INFO_DIR)
		$(CC) -Fo$@ -c $(CFLAGS) $*.c -FR$(BROWSE_INFO_DIR)/$*.sbr
else
		$(CC) -Fo$@ -c $(CFLAGS) $*.c
endif

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
ifdef GENERATE_BROWSE_INFO
	@mkdir $(BROWSE_INFO_DIR)
	$(CCC) -Fo$@ -c $(CFLAGS) $*.cpp -FR$(BROWSE_INFO_DIR)/$*.sbr
else
	$(CCC) -Fo$@ -c $(CFLAGS) $*.cpp
endif
else
	$(CCC) -o $@ -c $(CFLAGS) $*.cpp
endif
endif #STRICT_CPLUSPLUS_SUFFIX

%.i: %.cpp
	$(CCC) -C -E $(CFLAGS) $< > $*.i

%.i: %.c
	$(CC) -C -E $(CFLAGS) $< > $*.i

$(OBJDIR)/%: %.pl
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
	@list=`perl $(DEPTH)/config/core/outofdate.pl $(PERLARG)	\
		    -d $(JAVA_DESTPATH)/$(PACKAGE) $(JSRCS)`;	\
	if test "$$list"x != "x"; then				\
	    echo $(JAVAC) $$list;				\
	    $(JAVAC) $$list;					\
	fi

all:: export

clobber::
	rm -f $(XPDIST)/classes/$(PACKAGE)/*.class

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
			list=`perl $(DEPTH)/config/core/outofdate.pl $(PERLARG)	\
				    -d $(JAVA_DESTPATH)/$(PACKAGE) $$files`;	\
			if test "$${list}x" != "x"; thecoreconfn			\
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
	INCLUDES += -I$(JDK_GEN_DIR) -I$(XPDIST)
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
endif

#
# JRI_GEN -- for generating JRI native methods
#
# Generate JRI Headers and Stubs into the 'jri' directory
#
ifneq ($(JRI_GEN),)
ifdef NSBUILDROOT
	INCLUDES += -I$(JRI_GEN_DIR) -I$(XPDIST)
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
# Copy each element of EXPORTS to $(XPDIST)/public/$(MODULE)/
#
ifneq ($(EXPORTS),)
$(XPDIST)/public/$(MODULE)::
	@if test ! -d $@; then	    \
		echo Creating $@;   \
		$(NSINSTALL) -D $@; \
	fi

export:: $(EXPORTS) $(XPDIST)/public/$(MODULE)
	$(INSTALL) -m 444 $(EXPORTS) $(XPDIST)/public/$(MODULE)
endif

# Duplicate export rule for private exports, with different directories

ifneq ($(PRIVATE_EXPORTS),)
$(XPDIST)/private/$(MODULE)::
	@if test ! -d $@; then	    \
		echo Creating $@;   \
		$(NSINSTALL) -D $@; \
	fi

private_export:: $(PRIVATE_EXPORTS) $(XPDIST)/private/$(MODULE)
	$(INSTALL) -m 444 $(PRIVATE_EXPORTS) $(XPDIST)/private/$(MODULE)
else
private_export:: 
endif

# Duplicate export rule for releases, with different directories

ifneq ($(EXPORTS),)
$(XPDIST)/release/include::
	@if test ! -d $@; then	    \
		echo Creating $@;   \
		$(NSINSTALL) -D $@; \
	fi

release_export:: $(EXPORTS) $(XPDIST)/release/include
	$(INSTALL) -m 444 $(EXPORTS) $(XPDIST)/release/include
endif




################################################################################

ifneq ($(DEPENDENCIES),)
-include $(DEPENDENCIES)
endif

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

$(MKDEPENDENCIES): $(CSRCS) $(CPPSRCS) $(ASFILES) Makefile \
                   $(EXPORTS) $(LOCAL_EXPORTS)
	@$(MAKE_OBJDIR)
	touch $(MKDEPENDENCIES)
	$(MKDEPEND) -p$(OBJDIR_NAME)/ -o'$(OBJ_SUFFIX)' -f$(MKDEPENDENCIES) $(INCLUDES) $(SYS_INCLUDES) $(CSRCS) $(CPPSRCS) $(ASFILES)

$(MKDEPEND):
	cd $(MKDEPEND_DIR); $(MAKE)

ifdef OBJS
depend:: $(MKDEPEND) $(MKDEPENDENCIES)
else
depend::
endif
	+$(LOOP_OVER_DIRS)

dependclean::
	rm -f $(MKDEPENDENCIES)
	+$(LOOP_OVER_DIR)

# CUR_DIR is neccesary since it seems to crash gmake otherwise
CUR_DIR = $(shell pwd)

ifndef NO_IMPLICIT_DEPENDENCIES

ifdef OBJS
libs:: $(MKDEPENDENCIES)
endif

endif

ifneq ($(wildcard $(CUR_DIR)/$(OBJDIR)/depend.mk),)
-include $(CUR_DIR)/$(OBJDIR)/depend.mk
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

