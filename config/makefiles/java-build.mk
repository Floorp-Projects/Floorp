# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

ifndef INCLUDED_JAVA_BUILD_MK #{

ifdef JAVAFILES #{
GENERATED_DIRS += classes

export:: classes
classes: $(call mkdir_deps,classes)
endif #} JAVAFILES


ifdef ANDROID_APK_NAME #{
android_res_dirs := $(addprefix $(srcdir)/,$(or $(ANDROID_RES_DIRS),res))
_ANDROID_RES_FLAG := $(addprefix -S ,$(android_res_dirs))
_ANDROID_ASSETS_FLAG := $(addprefix -A ,$(ANDROID_ASSETS_DIR))

GENERATED_DIRS += classes

classes.dex: $(call mkdir_deps,classes)
classes.dex: R.java
classes.dex: $(ANDROID_APK_NAME).ap_
classes.dex: $(JAVAFILES)
	$(JAVAC) $(JAVAC_FLAGS) -d classes $(filter %.java,$^)
	$(DX) --dex --output=$@ classes $(ANDROID_EXTRA_JARS)

# R.java and $(ANDROID_APK_NAME).ap_ are both produced by aapt.  To
# save an aapt invocation, we produce them both at the same time.

R.java: .aapt.deps
$(ANDROID_APK_NAME).ap_: .aapt.deps

# This uses the fact that Android resource directories list all
# resource files one subdirectory below the parent resource directory.
android_res_files := $(wildcard $(addsuffix /*,$(wildcard $(addsuffix /*,$(android_res_dirs)))))

.aapt.deps: AndroidManifest.xml $(android_res_files) $(wildcard $(ANDROID_ASSETS_DIR))
	$(AAPT) package -f -M $< -I $(ANDROID_SDK)/android.jar $(_ANDROID_RES_FLAG) $(_ANDROID_ASSETS_FLAG) \
		-J ${@D} \
		-F $(ANDROID_APK_NAME).ap_
	@$(TOUCH) $@

$(ANDROID_APK_NAME)-unsigned-unaligned.apk: $(ANDROID_APK_NAME).ap_ classes.dex
	cp $< $@
	$(ZIP) -0 $@ classes.dex

$(ANDROID_APK_NAME)-unaligned.apk: $(ANDROID_APK_NAME)-unsigned-unaligned.apk
	cp $< $@
	$(DEBUG_JARSIGNER) $@

$(ANDROID_APK_NAME).apk: $(ANDROID_APK_NAME)-unaligned.apk
	$(ZIPALIGN) -f -v 4 $< $@

GARBAGE += \
  R.java \
  classes.dex  \
  $(ANDROID_APK_NAME).ap_ \
  $(ANDROID_APK_NAME)-unsigned-unaligned.apk \
  $(ANDROID_APK_NAME)-unaligned.apk \
  $(ANDROID_APK_NAME).apk \
  $(NULL)

JAVA_CLASSPATH := $(ANDROID_SDK)/android.jar
ifdef ANDROID_EXTRA_JARS #{
JAVA_CLASSPATH := $(JAVA_CLASSPATH):$(subst $(NULL) ,:,$(strip $(ANDROID_EXTRA_JARS)))
endif #} ANDROID_EXTRA_JARS

# Include Android specific java flags, instead of what's in rules.mk.
include $(topsrcdir)/config/android-common.mk
endif #} ANDROID_APK_NAME


ifdef JAVA_JAR_TARGETS #{
# Arg 1: Output target name with .jar suffix, like jars/jarfile.jar.
#        Intermediate class files are generated in jars/jarfile-classes.
# Arg 2: Java sources list.  We use VPATH and $^ so sources can be
#        relative to $(srcdir) or $(CURDIR).
# Arg 3: List of extra jars to link against.  We do not use VPATH so
#        jars must be relative to $(CURDIR).
# Arg 4: Additional JAVAC_FLAGS.
define java_jar_template
$(1): $(2) $(3)
	$$(REPORT_BUILD)
	@$$(NSINSTALL) -D $(1:.jar=)-classes
	@$$(if $$(filter-out .,$$(@D)),$$(NSINSTALL) -D $$(@D))
	$$(JAVAC) $$(JAVAC_FLAGS)\
    $(4)\
		-d $(1:.jar=)-classes\
		$(if $(strip $(3)),-classpath $(subst $(NULL) ,:,$(strip $(3))))\
		$$(filter %.java,$$^)
	$$(JAR) cMf $$@ -C $(1:.jar=)-classes .

GARBAGE += $(1)

GARBAGE_DIRS += $(1:.jar=)-classes
endef

$(foreach jar,$(JAVA_JAR_TARGETS),\
  $(if $($(jar)_DEST),,$(error Missing $(jar)_DEST))\
  $(if $($(jar)_JAVAFILES),,$(error Missing $(jar)_JAVAFILES))\
  $(eval $(call java_jar_template,$($(jar)_DEST),$($(jar)_JAVAFILES) $($(jar)_PP_JAVAFILES),$($(jar)_EXTRA_JARS),$($(jar)_JAVAC_FLAGS)))\
)
endif #} JAVA_JAR_TARGETS


INCLUDED_JAVA_BUILD_MK := 1

endif #} INCLUDED_JAVA_BUILD_MK
