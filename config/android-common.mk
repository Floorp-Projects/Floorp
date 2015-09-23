# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Ensure ANDROID_SDK is defined before including this file.
# We use common android defaults for boot class path and java version.
ifndef ANDROID_SDK
  $(error ANDROID_SDK must be defined before including android-common.mk)
endif

# DEBUG_JARSIGNER always debug signs.
DEBUG_JARSIGNER=$(PYTHON) $(abspath $(topsrcdir)/mobile/android/debug_sign_tool.py) \
  --keytool=$(KEYTOOL) \
  --jarsigner=$(JARSIGNER) \
  $(NULL)

# RELEASE_JARSIGNER release signs if possible.
ifdef MOZ_SIGN_CMD
RELEASE_JARSIGNER := $(MOZ_SIGN_CMD) -f jar
else
RELEASE_JARSIGNER := $(DEBUG_JARSIGNER)
endif

# $(1) is the full path to input:  foo-debug-unsigned-unaligned.apk.
# $(2) is the full path to output: foo.apk.
# Use this like: $(call RELEASE_SIGN_ANDROID_APK,foo-debug-unsigned-unaligned.apk,foo.apk)
RELEASE_SIGN_ANDROID_APK = \
  cp $(1) $(2)-unaligned.apk && \
  $(RELEASE_JARSIGNER) $(2)-unaligned.apk && \
  $(ZIPALIGN) -f -v 4 $(2)-unaligned.apk $(2) && \
  $(RM) $(2)-unaligned.apk

# For Android, we default to 1.7
ifndef JAVA_VERSION
  JAVA_VERSION = 1.7
endif

JAVAC_FLAGS = \
  -target $(JAVA_VERSION) \
  -source $(JAVA_VERSION) \
  -encoding UTF8 \
  -g:source,lines \
  -Werror \
  $(NULL)
