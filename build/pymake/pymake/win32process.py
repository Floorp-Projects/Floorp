# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from ctypes import windll, POINTER, byref, WinError
from ctypes.wintypes import WINFUNCTYPE, HANDLE, DWORD, BOOL

INFINITE = -1
WAIT_FAILED = 0xFFFFFFFF

LPDWORD = POINTER(DWORD)
_GetExitCodeProcessProto = WINFUNCTYPE(BOOL, HANDLE, LPDWORD)
_GetExitCodeProcess = _GetExitCodeProcessProto(("GetExitCodeProcess", windll.kernel32))
def GetExitCodeProcess(h):
    exitcode = DWORD()
    r = _GetExitCodeProcess(h, byref(exitcode))
    if r is 0:
        raise WinError()
    return exitcode.value

_WaitForMultipleObjectsProto = WINFUNCTYPE(DWORD, DWORD, POINTER(HANDLE), BOOL, DWORD)
_WaitForMultipleObjects = _WaitForMultipleObjectsProto(("WaitForMultipleObjects", windll.kernel32))

def WaitForAnyProcess(processes):
    arrtype = HANDLE * len(processes)
    harray = arrtype(*(int(p._handle) for p in processes))

    r = _WaitForMultipleObjects(len(processes), harray, False, INFINITE)
    if r == WAIT_FAILED:
        raise WinError()

    return processes[r], GetExitCodeProcess(int(processes[r]._handle)) <<8
