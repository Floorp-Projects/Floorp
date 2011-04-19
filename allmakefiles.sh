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

MAKEFILES=""

# add_makefiles - Shell function to add makefiles to MAKEFILES
add_makefiles() {
  MAKEFILES="$MAKEFILES $*"
}

if [ "$srcdir" = "" ]; then
  srcdir=.
fi

#
# Common makefiles used by everyone
#
add_makefiles "
Makefile
build/Makefile
build/pgo/Makefile
build/pgo/blueprint/Makefile
build/pgo/js-input/Makefile
build/unix/Makefile
build/win32/Makefile
build/win32/crashinjectdll/Makefile
config/Makefile
config/autoconf.mk
config/mkdepend/Makefile
config/nspr/Makefile
config/doxygen.cfg
config/expandlibs_config.py
config/tests/src-simple/Makefile
probes/Makefile
extensions/Makefile
"

if [ "$MOZ_MEMORY" -a "$LIBXUL_SDK" = "" ]; then
  add_makefiles "
    memory/jemalloc/Makefile
  "
fi

#
# Application-specific makefiles
#
if test -f "${srcdir}/${MOZ_BUILD_APP}/makefiles.sh"; then
  . "${srcdir}/${MOZ_BUILD_APP}/makefiles.sh"
fi

#
# Extension makefiles
#
for extension in $MOZ_EXTENSIONS; do
  if [ -f "${srcdir}/extensions/${extension}/makefiles.sh" ]; then
    . "${srcdir}/extensions/${extension}/makefiles.sh"
  fi
done

#
# Toolkit makefiles
#
if test -z "$LIBXUL_SDK"; then
  . "${srcdir}/toolkit/toolkit-makefiles.sh"
fi

# Services makefiles
. "${srcdir}/services/makefiles.sh"
