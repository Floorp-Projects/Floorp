#! /bin/sh
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

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
mozilla-config.h
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
mfbt/Makefile
probes/Makefile
extensions/Makefile
"

if [ "$MOZ_WEBAPP_RUNTIME" ]; then
  add_makefiles "
webapprt/Makefile
  "
fi

if [ ! "$LIBXUL_SDK" ]; then
  if [ "$STLPORT_SOURCES" ]; then
    add_makefiles "
      build/stlport/Makefile
      build/stlport/stl/config/_android.h
    "
  fi
  add_makefiles "
    memory/mozalloc/Makefile
    mozglue/Makefile
    mozglue/build/Makefile
  "
  if [ "$MOZ_JEMALLOC3" -o "$MOZ_REPLACE_MALLOC" ] && [ -z "$MOZ_NATIVE_JEMALLOC" ]; then
    add_makefiles "
      memory/jemalloc/Makefile
    "
  fi
  if [ "$MOZ_MEMORY" ]; then
    add_makefiles "
      memory/Makefile
      memory/mozjemalloc/Makefile
      memory/build/Makefile
    "
  fi
  if [ "$MOZ_REPLACE_MALLOC" ]; then
    add_makefiles "
      memory/replace/Makefile
    "
    if [ -z "$MOZ_JEMALLOC3" ]; then
      add_makefiles "
        memory/replace/jemalloc/Makefile
      "
    fi
  fi
  if [ "$MOZ_REPLACE_MALLOC_LINKAGE" = "dummy library" ]; then
    add_makefiles "
      memory/replace/dummy/Makefile
    "
  fi
  if [ "$MOZ_WIDGET_TOOLKIT" = "android" ]; then
    add_makefiles "
      other-licenses/android/Makefile
      other-licenses/skia-npapi/Makefile
      mozglue/android/Makefile
    "
  fi
  if [ "$MOZ_LINKER" ]; then
    add_makefiles "
      mozglue/linker/Makefile
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
  if [ "$STDCXX_COMPAT" ]; then
    add_makefiles "
      build/unix/stdc++compat/Makefile
    "
  fi
  if [ "$USE_ELF_HACK" ]; then
    add_makefiles "
      build/unix/elfhack/Makefile
    "
  fi
fi

if [ "$ENABLE_MARIONETTE" ]; then
  add_makefiles "
    testing/marionette/Makefile
    testing/marionette/components/Makefile
  "
fi

if [ "$ENABLE_TESTS" ]; then
  add_makefiles "
    config/makefiles/test/Makefile
    config/tests/makefiles/autodeps/Makefile
    config/tests/src-simple/Makefile
    mfbt/tests/Makefile
  "
  if [ ! "$LIBXUL_SDK" ]; then 
    add_makefiles "
      mozglue/tests/Makefile
    "
  fi
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
      build/mobile/robocop/Makefile
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
