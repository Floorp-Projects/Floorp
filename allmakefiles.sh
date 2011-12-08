#! /bin/sh
#
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
# Portions created by the Initial Developer are Copyright (C) 1999
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

# allmakefiles.sh - List of all makefiles.
#   Appends the list of makefiles to the variable, MAKEFILES.
#   There is no need to rerun autoconf after adding makefiles.
#   You only need to run configure.

# Turn on exit on error
set -o errexit

MAKEFILES=""

# add_makefiles - Shell function to add makefiles to MAKEFILES
add_makefiles() {
  MAKEFILES="$MAKEFILES $*"
}

if [ "$srcdir" = "" ]; then
  srcdir=.
fi

# Common makefiles used by everyone
add_makefiles "
Makefile
build/Makefile
build/pgo/Makefile
build/pgo/blueprint/Makefile
build/pgo/js-input/Makefile
config/Makefile
config/autoconf.mk
config/nspr/Makefile
config/doxygen.cfg
config/expandlibs_config.py
config/tests/src-simple/Makefile
probes/Makefile
extensions/Makefile
"

if [ ! "$LIBXUL_SDK" ]; then
  add_makefiles "
    memory/Makefile
    memory/mozalloc/Makefile
    memory/mozutils/Makefile
  "
  if [ "$MOZ_MEMORY" ]; then
    add_makefiles "
      memory/jemalloc/Makefile
    "
  fi
  if [ "$MOZ_WIDGET_TOOLKIT" = "android" ]; then
    add_makefiles "
      other-licenses/android/Makefile
      other-licenses/skia-npapi/Makefile
    "
  fi
fi

if [ "$OS_ARCH" = "WINNT" ]; then
  add_makefiles "
    build/win32/Makefile
    build/win32/crashinjectdll/Makefile
  "
fi

if [ "$OS_ARCH" != "WINNT" -a "$OS_ARCH" != "OS2" ]; then
  add_makefiles "
    build/unix/Makefile
  "
  if [ "$USE_ELF_HACK" ]; then
    add_makefiles "
      build/unix/elfhack/Makefile
    "
  fi
fi

if [ "$COMPILER_DEPEND" = "" -a "$MOZ_NATIVE_MAKEDEPEND" = "" ]; then
  add_makefiles "
    config/mkdepend/Makefile
  "
fi

if [ "$ENABLE_TESTS" ]; then
  add_makefiles "
    build/autoconf/test/Makefile
  "
  if [ "$_MSC_VER" -a "$OS_TEST" != "x86_64" ]; then
    add_makefiles "
      build/win32/vmwarerecordinghelper/Makefile
    "
  fi
  if [ "$OS_ARCH" != "WINNT" -a "$OS_ARCH" != "OS2" ]; then 
    add_makefiles "
      build/unix/test/Makefile
    "
  fi
  if [ "$MOZ_WIDGET_TOOLKIT" = "android" ]; then
    add_makefiles "
      build/mobile/sutagent/android/Makefile
      build/mobile/sutagent/android/fencp/Makefile
      build/mobile/sutagent/android/ffxcp/Makefile
      build/mobile/sutagent/android/watcher/Makefile
    "
  fi
fi

# Application-specific makefiles
if [ -f "${srcdir}/${MOZ_BUILD_APP}/makefiles.sh" ]; then
  . "${srcdir}/${MOZ_BUILD_APP}/makefiles.sh"
fi

# Extension makefiles
for extension in $MOZ_EXTENSIONS; do
  if [ -f "${srcdir}/extensions/${extension}/makefiles.sh" ]; then
    . "${srcdir}/extensions/${extension}/makefiles.sh"
  fi
done

# Toolkit makefiles
if [ ! "$LIBXUL_SDK" ]; then
  . "${srcdir}/toolkit/toolkit-makefiles.sh"
fi

# Services makefiles
. "${srcdir}/services/makefiles.sh"

# Turn off exit on error, since it breaks the rest of configure
set +o errexit
