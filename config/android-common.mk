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
# The Original Code is Android Java Make Settings.
#
# The Initial Developer of the Original Code is
# Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Clint Talbert <cmtalbert@gmail.com>
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

# Ensure JAVA_CLASSPATH and ANDROID_SDK are defined before including this file.
# We use common android defaults for boot class path and java version.
ifndef ANDROID_SDK
  $(error ANDROID_SDK must be defined before including android-common.mk)
endif

ifndef JAVA_CLASSPATH
  $(error JAVA_CLASSPATH must be defined before including android-common.mk)
endif

DX=$(ANDROID_PLATFORM_TOOLS)/dx
AAPT=$(ANDROID_PLATFORM_TOOLS)/aapt
APKBUILDER=$(ANDROID_SDK)/../../tools/apkbuilder
ZIPALIGN=$(ANDROID_SDK)/../../tools/zipalign

ifdef JARSIGNER
  APKBUILDER_FLAGS += -u
endif

# For Android, this defaults to $(ANDROID_SDK)/android.jar
ifndef JAVA_BOOTCLASSPATH
  JAVA_BOOTCLASSPATH = $(ANDROID_SDK)/android.jar
endif

# For Android, we default to 1.5
ifndef JAVA_VERSION
  JAVA_VERSION = 1.5
endif

JAVAC_FLAGS = \
  -target $(JAVA_VERSION) \
  -source $(JAVA_VERSION) \
  -classpath $(JAVA_CLASSPATH) \
  -bootclasspath $(JAVA_BOOTCLASSPATH) \
  -encoding ascii \
  -g \
  $(NULL)

