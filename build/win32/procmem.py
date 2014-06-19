# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
