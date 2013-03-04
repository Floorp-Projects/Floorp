# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Prints the pre-release version suffix based on the version string
#
# Examples:
# 2.1a3    > " 2.1 Alpha 3"
# 2.1a3pre > ""
# 3.2b4    > " 3.2 Beta 4"
# 3.2b4pre > ""
from __future__ import print_function

import sys
import re

def get_prerelease_suffix(version):
  """ Returns the prerelease suffix from the version string argument """

  def mfunc(m):
    return " {0} {1} {2}".format(m.group('prefix'),
                                 {'a': 'Alpha', 'b': 'Beta'}[m.group('c')],
                                 m.group('suffix'))
  result, c = re.subn(r'^(?P<prefix>(\d+\.)*\d+)(?P<c>[ab])(?P<suffix>\d+)$',
                      mfunc, version)
  if c != 1:
    return ''
  return result

if len(sys.argv) == 2:
  print(get_prerelease_suffix(sys.argv[1]))
