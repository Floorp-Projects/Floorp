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

# For Android, this defaults to $(ANDROID_SDK)/android.jar
ifndef JAVA_BOOTCLASSPATH
  JAVA_BOOTCLASSPATH = $(ANDROID_SDK)/android.jar
endif

# For Android, we default to 1.7
ifndef JAVA_VERSION
  JAVA_VERSION = 1.7
endif

JAVAC_FLAGS = \
  -target $(JAVA_VERSION) \
  -source $(JAVA_VERSION) \
  $(if $(JAVA_CLASSPATH),-classpath $(JAVA_CLASSPATH),) \
  -bootclasspath $(JAVA_BOOTCLASSPATH) \
  -encoding UTF8 \
  -g:source,lines \
  -Werror \
  $(NULL)
