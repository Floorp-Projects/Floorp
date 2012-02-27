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
# The Original Code is mozilla.org
#
# The Initial Developer of the Original Code is
# the Mozilla Foundation
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
# Mike Hommey <mh@glandium.org>
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

# This script returns the number of items for the DW_AT_ranges corresponding
# to a given compilation unit. This is used as a helper to find a bug in some
# versions of GNU ld.

import subprocess
import sys
import re

def get_range_for(compilation_unit, debug_info):
    '''Returns the range offset for a given compilation unit
       in a given debug_info.'''
    name = ranges = ''
    search_cu = False
    for nfo in debug_info.splitlines():
        if 'DW_TAG_compile_unit' in nfo:
            search_cu = True
        elif 'DW_TAG_' in nfo or not nfo.strip():
            if name == compilation_unit:
                return int(ranges, 16)
            name = ranges = ''
            search_cu = False
        if search_cu:
            if 'DW_AT_name' in nfo:
                name = nfo.rsplit(None, 1)[1]
            elif 'DW_AT_ranges' in nfo:
                ranges = nfo.rsplit(None, 1)[1]
    return None

def get_range_length(range, debug_ranges):
    '''Returns the number of items in the range starting at the
       given offset.'''
    length = 0
    for line in debug_ranges.splitlines():
        m = re.match('\s*([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)', line)
        if m and int(m.group(1), 16) == range:
            length += 1
    return length

def main(bin, compilation_unit):
    p = subprocess.Popen(['objdump', '-W', bin], stdout = subprocess.PIPE, stderr = subprocess.PIPE)
    (out, err) = p.communicate()
    sections = re.split('\n(Contents of the|The section) ', out)
    debug_info = [s for s in sections if s.startswith('.debug_info')]
    debug_ranges = [s for s in sections if s.startswith('.debug_ranges')]
    if not debug_ranges or not debug_info:
        return 0

    range = get_range_for(compilation_unit, debug_info[0])
    if range is not None:
        return get_range_length(range, debug_ranges[0])

    return -1


if __name__ == '__main__':
    print main(*sys.argv[1:])
