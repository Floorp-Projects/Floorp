# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

ifndef INCLUDED_JAVA_BUILD_MK #{

ifdef ANDROID_RESFILES #{
res-dep := .deps-copy-java-res

GENERATED_DIRS += res
GARBAGE        += $(res-dep)

export:: $(res-dep)

res-dep-preqs := \
  $(addprefix $(srcdir)/,$(ANDROID_RESFILES)) \
  $(call mkdir_deps,res) \
  $(if $(IS_LANGUAGE_REPACK),FORCE) \
  $(NULL)

# nop-build: only copy res/ files when needed
$(res-dep): $(res-dep-preqs)
	$(call copy_dir,$(srcdir)/res,$(CURDIR)/res)
	@$(TOUCH) $@
endif #} ANDROID_RESFILES


ifdef JAVAFILES #{
GENERATED_DIRS += classes

export:: classes
classes: $(call mkdir_deps,classes)
endif #} JAVAFILES


ifdef ANDROID_APK_NAME #{
_ANDROID_RES_FLAG := -S $(or $(ANDROID_RES_DIR),res)
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

.aapt.deps: AndroidManifest.xml $(wildcard $(ANDROID_RES_DIR)) $(wildcard $(ANDROID_ASSETS_DIR))
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

INCLUDED_JAVA_BUILD_MK := 1

endif #} INCLUDED_JAVA_BUILD_MK
