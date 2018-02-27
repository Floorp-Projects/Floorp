# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

ifndef INCLUDED_JAVA_BUILD_MK #{

default_bootclasspath_jars := \
  $(ANDROID_SDK)/android.jar \
  $(NULL)

default_classpath_jars := \
  $(NULL)

# Turn a possibly empty list of JAR files into a Java classpath, like a.jar:b.jar.
# Arg 1: Possibly empty list of JAR files.
define classpath_template
$(subst $(NULL) ,:,$(strip $(1)))
endef

ifdef JAVA_JAR_TARGETS #{
# Arg 1: Output target name with .jar suffix, like jars/jarfile.jar.
#        Intermediate class files are generated in jars/jarfile-classes.
# Arg 2: Java sources list.  We use VPATH and $^ so sources can be
#        relative to $(srcdir) or $(CURDIR).
# Arg 3: List of extra jars to link against.  We do not use VPATH so
#        jars must be relative to $(CURDIR).
# Arg 4: Additional JAVAC_FLAGS.

# Note: Proguard fails when stale .class files corresponding to
# removed inner classes are present in the object directory.  These
# stale class files get packaged into the .jar file, which then gets
# processed by Proguard.  To work around this, we always delete any
# existing jarfile-classes directory and start fresh.

define java_jar_template
$(1): $(2) $(3) $(default_bootclasspath_jars) $(default_classpath_jars)
	$$(REPORT_BUILD)
	@$$(RM) -rf $(1:.jar=)-classes
	@$$(NSINSTALL) -D $(1:.jar=)-classes
	@$$(if $$(filter-out .,$$(@D)),$$(NSINSTALL) -D $$(@D))
	$$(JAVAC) $$(JAVAC_FLAGS)\
		$(4)\
		-d $(1:.jar=)-classes\
		$(addprefix -bootclasspath ,$(call classpath_template,$(default_bootclasspath_jars)))\
		$(addprefix -classpath ,$(call classpath_template,$(default_classpath_jars) $(3)))\
		$$(filter %.java,$$^)
	$$(JAR) cMf $$@ -C $(1:.jar=)-classes .

GARBAGE += $(1)

GARBAGE_DIRS += $(1:.jar=)-classes
endef

$(foreach jar,$(JAVA_JAR_TARGETS),\
  $(if $($(jar)_DEST),,$(error Missing $(jar)_DEST))\
  $(if $($(jar)_JAVAFILES) $($(jar)_PP_JAVAFILES),,$(error Must provide at least one of $(jar)_JAVAFILES and $(jar)_PP_JAVAFILES))\
  $(eval $(call java_jar_template,$($(jar)_DEST),$($(jar)_JAVAFILES) $($(jar)_PP_JAVAFILES),$($(jar)_EXTRA_JARS),$($(jar)_JAVAC_FLAGS)))\
)
endif #} JAVA_JAR_TARGETS


INCLUDED_JAVA_BUILD_MK := 1

endif #} INCLUDED_JAVA_BUILD_MK
