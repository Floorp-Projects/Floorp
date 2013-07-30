# A module to expose various thread/process/job related structures and
# methods from kernel32
#
# The MIT License
#
# Copyright (c) 2003-2004 by Peter Astrand <astrand@lysator.liu.se>
#
# Additions and modifications written by Benjamin Smedberg
# <benjamin@smedbergs.us> are Copyright (c) 2006 by the Mozilla Foundation
# <http://www.mozilla.org/>
#
# More Modifications
# Copyright (c) 2006-2007 by Mike Taylor <bear@code-bear.com>
# Copyright (c) 2007-2008 by Mikeal Rogers <mikeal@mozilla.com>
#
# By obtaining, using, and/or copying this software and/or its
# associated documentation, you agree that you have read, understood,
# and will comply with the following terms and conditions:
#
# Permission to use, copy, modify, and distribute this software and
# its associated documentation for any purpose and without fee is
# hereby granted, provided that the above copyright notice appears in
# all copies, and that both that copyright notice and this permission
# notice appear in supporting documentation, and that the name of the
# author not be used in advertising or publicity pertaining to
# distribution of the software without specific, written prior
# permission.
#
# THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
# INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, INDIRECT OR
# CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
# OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
# WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

from ctypes import c_void_p, POINTER, sizeof, Structure, windll, WinError, WINFUNCTYPE
from ctypes.wintypes import BOOL, BYTE, DWORD, HANDLE, LPCWSTR, LPWSTR, UINT, WORD, \
                            c_buffer, c_ulong, byref
from qijo import QueryInformationJobObject

LPVOID = c_void_p
LPBYTE = POINTER(BYTE)
LPDWORD = POINTER(DWORD)
LPBOOL = POINTER(BOOL)

def ErrCheckBool(result, func, args):
    """errcheck function for Windows functions that return a BOOL True
    on success"""
    if not result:
        raise WinError()
    return args


# AutoHANDLE

class AutoHANDLE(HANDLE):
    """Subclass of HANDLE which will call CloseHandle() on deletion."""
    
    CloseHandleProto = WINFUNCTYPE(BOOL, HANDLE)
    CloseHandle = CloseHandleProto(("CloseHandle", windll.kernel32))
    CloseHandle.errcheck = ErrCheckBool
    
    def Close(self):
        if self.value and self.value != HANDLE(-1).value:
            self.CloseHandle(self)
            self.value = 0
    
    def __del__(self):
        self.Close()

    def __int__(self):
        return self.value

def ErrCheckHandle(result, func, args):
    """errcheck function for Windows functions that return a HANDLE."""
    if not result:
        raise WinError()
    return AutoHANDLE(result)

# PROCESS_INFORMATION structure

class PROCESS_INFORMATION(Structure):
    _fields_ = [("hProcess", HANDLE),
                ("hThread", HANDLE),
                ("dwProcessID", DWORD),
                ("dwThreadID", DWORD)]

    def __init__(self):
        Structure.__init__(self)
        
        self.cb = sizeof(self)

LPPROCESS_INFORMATION = POINTER(PROCESS_INFORMATION)

# STARTUPINFO structure

class STARTUPINFO(Structure):
    _fields_ = [("cb", DWORD),
                ("lpReserved", LPWSTR),
                ("lpDesktop", LPWSTR),
                ("lpTitle", LPWSTR),
                ("dwX", DWORD),
                ("dwY", DWORD),
                ("dwXSize", DWORD),
                ("dwYSize", DWORD),
                ("dwXCountChars", DWORD),
                ("dwYCountChars", DWORD),
                ("dwFillAttribute", DWORD),
                ("dwFlags", DWORD),
                ("wShowWindow", WORD),
                ("cbReserved2", WORD),
                ("lpReserved2", LPBYTE),
                ("hStdInput", HANDLE),
                ("hStdOutput", HANDLE),
                ("hStdError", HANDLE)
                ]
LPSTARTUPINFO = POINTER(STARTUPINFO)

SW_HIDE                 = 0

STARTF_USESHOWWINDOW    = 0x01
STARTF_USESIZE          = 0x02
STARTF_USEPOSITION      = 0x04
STARTF_USECOUNTCHARS    = 0x08
STARTF_USEFILLATTRIBUTE = 0x10
STARTF_RUNFULLSCREEN    = 0x20
STARTF_FORCEONFEEDBACK  = 0x40
STARTF_FORCEOFFFEEDBACK = 0x80
STARTF_USESTDHANDLES    = 0x100

# EnvironmentBlock

class EnvironmentBlock:
    """An object which can be passed as the lpEnv parameter of CreateProcess.
    It is initialized with a dictionary."""

    def __init__(self, dict):
        if not dict:
            self._as_parameter_ = None
        else:
            values = ["%s=%s" % (key, value)
                      for (key, value) in dict.iteritems()]
            values.append("")
            self._as_parameter_ = LPCWSTR("\0".join(values))
        
# CreateProcess()

CreateProcessProto = WINFUNCTYPE(BOOL,                  # Return type
                                 LPCWSTR,               # lpApplicationName
                                 LPWSTR,                # lpCommandLine
                                 LPVOID,                # lpProcessAttributes
                                 LPVOID,                # lpThreadAttributes
                                 BOOL,                  # bInheritHandles
                                 DWORD,                 # dwCreationFlags
                                 LPVOID,                # lpEnvironment
                                 LPCWSTR,               # lpCurrentDirectory
                                 LPSTARTUPINFO,         # lpStartupInfo
                                 LPPROCESS_INFORMATION  # lpProcessInformation
                                 )

CreateProcessFlags = ((1, "lpApplicationName", None),
                      (1, "lpCommandLine"),
                      (1, "lpProcessAttributes", None),
                      (1, "lpThreadAttributes", None),
                      (1, "bInheritHandles", True),
                      (1, "dwCreationFlags", 0),
                      (1, "lpEnvironment", None),
                      (1, "lpCurrentDirectory", None),
                      (1, "lpStartupInfo"),
                      (2, "lpProcessInformation"))

def ErrCheckCreateProcess(result, func, args):
    ErrCheckBool(result, func, args)
    # return a tuple (hProcess, hThread, dwProcessID, dwThreadID)
    pi = args[9]
    return AutoHANDLE(pi.hProcess), AutoHANDLE(pi.hThread), pi.dwProcessID, pi.dwThreadID

CreateProcess = CreateProcessProto(("CreateProcessW", windll.kernel32),
                                   CreateProcessFlags)
CreateProcess.errcheck = ErrCheckCreateProcess

# flags for CreateProcess
CREATE_BREAKAWAY_FROM_JOB = 0x01000000
CREATE_DEFAULT_ERROR_MODE = 0x04000000
CREATE_NEW_CONSOLE = 0x00000010
CREATE_NEW_PROCESS_GROUP = 0x00000200
CREATE_NO_WINDOW = 0x08000000
CREATE_SUSPENDED = 0x00000004
CREATE_UNICODE_ENVIRONMENT = 0x00000400

# flags for job limit information
# see http://msdn.microsoft.com/en-us/library/ms684147%28VS.85%29.aspx
JOB_OBJECT_LIMIT_BREAKAWAY_OK = 0x00000800
JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK = 0x00001000

# XXX these flags should be documented
DEBUG_ONLY_THIS_PROCESS = 0x00000002
DEBUG_PROCESS = 0x00000001
DETACHED_PROCESS = 0x00000008

# CreateJobObject()

CreateJobObjectProto = WINFUNCTYPE(HANDLE,             # Return type
                                   LPVOID,             # lpJobAttributes
                                   LPCWSTR             # lpName
                                   )

CreateJobObjectFlags = ((1, "lpJobAttributes", None),
                        (1, "lpName", None))

CreateJobObject = CreateJobObjectProto(("CreateJobObjectW", windll.kernel32),
                                       CreateJobObjectFlags)
CreateJobObject.errcheck = ErrCheckHandle

# AssignProcessToJobObject()

AssignProcessToJobObjectProto = WINFUNCTYPE(BOOL,      # Return type
                                            HANDLE,    # hJob
                                            HANDLE     # hProcess
                                            )
AssignProcessToJobObjectFlags = ((1, "hJob"),
                                 (1, "hProcess"))
AssignProcessToJobObject = AssignProcessToJobObjectProto(
    ("AssignProcessToJobObject", windll.kernel32),
    AssignProcessToJobObjectFlags)
AssignProcessToJobObject.errcheck = ErrCheckBool

# GetCurrentProcess()
# because os.getPid() is way too easy
GetCurrentProcessProto = WINFUNCTYPE(HANDLE    # Return type
                                     )
GetCurrentProcessFlags = ()
GetCurrentProcess = GetCurrentProcessProto(
    ("GetCurrentProcess", windll.kernel32),
    GetCurrentProcessFlags)
GetCurrentProcess.errcheck = ErrCheckHandle

# IsProcessInJob()
try:
    IsProcessInJobProto = WINFUNCTYPE(BOOL,     # Return type
                                      HANDLE,   # Process Handle
                                      HANDLE,   # Job Handle
                                      LPBOOL      # Result
                                      )
    IsProcessInJobFlags = ((1, "ProcessHandle"),
                           (1, "JobHandle", HANDLE(0)),
                           (2, "Result"))
    IsProcessInJob = IsProcessInJobProto(
        ("IsProcessInJob", windll.kernel32),
        IsProcessInJobFlags)
    IsProcessInJob.errcheck = ErrCheckBool 
except AttributeError:
    # windows 2k doesn't have this API
    def IsProcessInJob(process):
        return False


# ResumeThread()

def ErrCheckResumeThread(result, func, args):
    if result == -1:
        raise WinError()

    return args

ResumeThreadProto = WINFUNCTYPE(DWORD,      # Return type
                                HANDLE      # hThread
                                )
ResumeThreadFlags = ((1, "hThread"),)
ResumeThread = ResumeThreadProto(("ResumeThread", windll.kernel32),
                                 ResumeThreadFlags)
ResumeThread.errcheck = ErrCheckResumeThread

# TerminateProcess()

TerminateProcessProto = WINFUNCTYPE(BOOL,   # Return type
                                    HANDLE, # hProcess
                                    UINT    # uExitCode
                                    )
TerminateProcessFlags = ((1, "hProcess"),
                         (1, "uExitCode", 127))
TerminateProcess = TerminateProcessProto(
    ("TerminateProcess", windll.kernel32),
    TerminateProcessFlags)
TerminateProcess.errcheck = ErrCheckBool

# TerminateJobObject()

TerminateJobObjectProto = WINFUNCTYPE(BOOL,   # Return type
                                      HANDLE, # hJob
                                      UINT    # uExitCode
                                      )
TerminateJobObjectFlags = ((1, "hJob"),
                           (1, "uExitCode", 127))
TerminateJobObject = TerminateJobObjectProto(
    ("TerminateJobObject", windll.kernel32),
    TerminateJobObjectFlags)
TerminateJobObject.errcheck = ErrCheckBool

# WaitForSingleObject()

WaitForSingleObjectProto = WINFUNCTYPE(DWORD,  # Return type
                                       HANDLE, # hHandle
                                       DWORD,  # dwMilliseconds
                                       )
WaitForSingleObjectFlags = ((1, "hHandle"),
                            (1, "dwMilliseconds", -1))
WaitForSingleObject = WaitForSingleObjectProto(
    ("WaitForSingleObject", windll.kernel32),
    WaitForSingleObjectFlags)

INFINITE = -1
WAIT_TIMEOUT = 0x0102
WAIT_OBJECT_0 = 0x0
WAIT_ABANDONED = 0x0080
WAIT_FAILED = 0xFFFFFFFF

# GetExitCodeProcess()

GetExitCodeProcessProto = WINFUNCTYPE(BOOL,    # Return type
                                      HANDLE,  # hProcess
                                      LPDWORD, # lpExitCode
                                      )
GetExitCodeProcessFlags = ((1, "hProcess"),
                           (2, "lpExitCode"))
GetExitCodeProcess = GetExitCodeProcessProto(
    ("GetExitCodeProcess", windll.kernel32),
    GetExitCodeProcessFlags)
GetExitCodeProcess.errcheck = ErrCheckBool

def CanCreateJobObject():
    # Running firefox in a job (from cfx) hangs on sites using flash plugin
    # so job creation is turned off for now. (see Bug 768651).
    return False

### testing functions

def parent():
    print 'Starting parent'
    currentProc = GetCurrentProcess()
    if IsProcessInJob(currentProc):
        print >> sys.stderr, "You should not be in a job object to test"
        sys.exit(1)
    assert CanCreateJobObject()
    print 'File: %s' % __file__
    command = [sys.executable, __file__, '-child']
    print 'Running command: %s' % command
    process = Popen(command)
    process.kill()
    code = process.returncode
    print 'Child code: %s' % code
    assert code == 127
        
def child():
    print 'Starting child'
    currentProc = GetCurrentProcess()
    injob = IsProcessInJob(currentProc)
    print "Is in a job?: %s" % injob
    can_create = CanCreateJobObject()
    print 'Can create job?: %s' % can_create
    process = Popen('c:\\windows\\notepad.exe')
    assert process._job
    jobinfo = QueryInformationJobObject(process._job, 'JobObjectExtendedLimitInformation')
    print 'Job info: %s' % jobinfo
    limitflags = jobinfo['BasicLimitInformation']['LimitFlags']
    print 'LimitFlags: %s' % limitflags
    process.kill()

if __name__ == '__main__':
    import sys
    from killableprocess import Popen
    nargs = len(sys.argv[1:])
    if nargs:
        if nargs != 1 or sys.argv[1] != '-child':
            raise AssertionError('Wrong flags; run like `python /path/to/winprocess.py`')
        child()
    else:
        parent()
