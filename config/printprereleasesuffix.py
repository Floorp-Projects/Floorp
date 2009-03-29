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
# The Original Code is Mozilla.
#
# The Initial Developer of the Original Code is
# the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2009
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Robert Strong <robert.bugzilla@gmail.com>
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

# Prints the pre-release version suffix based on the version string
#
# Examples:
# 2.1a3    > " 2.1 Alpha 3"
# 2.1a3pre > ""
# 3.2b4    > " 3.2 Beta 4"
# 3.2b4pre > ""

import sys
import re

def get_prerelease_suffix(version):
  """ Returns the prerelease suffix from the version string argument """

  def mfunc(m):
    return " %s %s %s" % (m.group('prefix'),
                         {'a': 'Alpha', 'b': 'Beta'}[m.group('c')],
                         m.group('suffix'))
  result, c = re.subn(r'^(?P<prefix>(\d+\.)*\d+)(?P<c>[ab])(?P<suffix>\d+)$',
                      mfunc, version)
  if c != 1:
    return ''
  return result

if len(sys.argv) == 2:
  print get_prerelease_suffix(sys.argv[1])
