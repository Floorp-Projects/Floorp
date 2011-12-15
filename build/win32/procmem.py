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
# The Original Code is cl.py: a python wrapper for cl to automatically generate
# dependency information.
#
# The Initial Developer of the Original Code is
#   Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ted Mielczarek <ted@mielczarek.org>
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

import os, sys, ctypes, ctypes.wintypes

class VM_COUNTERS(ctypes.Structure):
    _fields_ = [("PeakVirtualSize", ctypes.wintypes.ULONG),
                ("VirtualSize", ctypes.wintypes.ULONG),
                ("PageFaultCount", ctypes.wintypes.ULONG),
                ("PeakWorkingSetSize", ctypes.wintypes.ULONG),
                ("WorkingSetSize", ctypes.wintypes.ULONG),
                ("QuotaPeakPagedPoolUsage", ctypes.wintypes.ULONG),
                ("QuotaPagedPoolUsage", ctypes.wintypes.ULONG),
                ("QuotaPeakNonPagedPoolUsage", ctypes.wintypes.ULONG),
                ("QuotaNonPagedPoolUsage", ctypes.wintypes.ULONG),
                ("PagefileUsage", ctypes.wintypes.ULONG),
                ("PeakPagefileUsage", ctypes.wintypes.ULONG)
                ]

def get_vmsize(handle):
    """
    Return (peak_virtual_size, virtual_size) for the process |handle|.
    """
    ProcessVmCounters = 3
    vmc = VM_COUNTERS()
    if ctypes.windll.ntdll.NtQueryInformationProcess(int(handle),
                                                     ProcessVmCounters,
                                                     ctypes.byref(vmc),
                                                     ctypes.sizeof(vmc),
                                                     None) == 0:
       return (vmc.PeakVirtualSize, vmc.VirtualSize)

    return (-1, -1)

if __name__ == '__main__':
    PROCESS_QUERY_INFORMATION = 0x0400
    for pid in sys.argv[1:]:
        handle = ctypes.windll.kernel32.OpenProcess(PROCESS_QUERY_INFORMATION,
                                                    0, # no inherit
                                                    int(pid))
        if handle:
            print "Process %s:" % pid
            vsize, peak_vsize = get_vmsize(handle)
            print "peak vsize: %d" % peak_vsize
            ctypes.windll.kernel32.CloseHandle(handle)
        else:
            print "Couldn't open process %s" % pid
