#!/bin/bash

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
# The Original Code is JavaScript Engine testing utilities.
#
# The Initial Developer of the Original Code is
# Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s): Bob Clary
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

TEST_BIN=$1
TEST_TIMEOUT=$2
TEST_EXE=$3
TEST_PROFILE=$4

find ecma ecma_2 ecma_3 js1_1 js1_2 js1_3 js1_4 js1_5 js1_6 -mindepth 2 -name '*.js' -print | \
grep -v shell.js | \
grep -v browser.js | \
grep -v template.js | \
sed 's/^\.\///' | \
while read jsfile
do
  echo jsbrowsereach Begin JavaScript Test: $jsfile

  if ! ./each.pl $TEST_BIN $TEST_TIMEOUT $TEST_EXE $TEST_PROFILE $jsfile userhookeach-js.js 2>&1 | eval "sed 's@^@jsbrowsereach:"$jsfile": @'"; then
      rc=$?
      echo "$0: ./each.pl terminated with exitcode $rc"
      exit $rc
  fi
  echo jsbrowsereach End JavaScript Test: $jsfile
done
