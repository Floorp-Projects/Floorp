/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "MPL"); you may not use this file
 * except in compliance with the MPL. You may obtain a copy of
 * the MPL at http://www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the MPL for the specific language governing
 * rights and limitations under the MPL.
 *
 * The Original Code is subprocess.jsm.
 *
 * The Initial Developer of this code is Patrick Brunschwig.
 * Portions created by Patrick Brunschwig <patrick@enigmail.net>
 * are Copyright (C) 2011 Patrick Brunschwig.
 * All Rights Reserved.
 *
 * Contributor(s):
 * Jan Gerber <j@mailb.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 * ***** END LICENSE BLOCK ***** */

/*
 * ChromeWorker Object subprocess.jsm on Windows to process stdin/stdout/stderr
 * on separate threads.
 *
 */

// Being a ChromeWorker object, implicitly uses the following:
// Cu.import("resource://gre/modules/ctypes.jsm");

'use strict';

const BufferSize = 1024;

const BOOL = ctypes.bool;
const HANDLE = ctypes.size_t;
const DWORD = ctypes.uint32_t;
const LPDWORD = DWORD.ptr;
const PVOID = ctypes.voidptr_t;
const LPVOID = PVOID;

/*
typedef struct _OVERLAPPED {
  ULONG_PTR Internal;
  ULONG_PTR InternalHigh;
  union {
    struct {
      DWORD Offset;
      DWORD OffsetHigh;
    };
    PVOID  Pointer;
  };
  HANDLE    hEvent;
} OVERLAPPED, *LPOVERLAPPED;
*/
const OVERLAPPED = new ctypes.StructType("OVERLAPPED");

var ReadFileBuffer = ctypes.char.array(BufferSize);
var WriteFileBuffer = ctypes.uint8_t.array(BufferSize);

var kernel32dll = null;
var libFunc = {};

function initLib(libName) {
    if (ctypes.size_t.size == 8) {
        var WinABI = ctypes.default_abi;
    } else {
        var WinABI = ctypes.winapi_abi;
    }

    kernel32dll = ctypes.open(libName);

    /*
    BOOL WINAPI WriteFile(
      __in         HANDLE hFile,
      __in         LPCVOID lpBuffer,
      __in         DWORD nNumberOfBytesToWrite,
      __out_opt    LPDWORD lpNumberOfBytesWritten,
      __inout_opt  LPOVERLAPPED lpOverlapped
    );

    NOTE: lpBuffer is declared as array of unsigned int8 instead of char to avoid
           implicit charset conversion
    */
    libFunc.WriteFile = kernel32dll.declare("WriteFile",
                                        WinABI,
                                        BOOL,
                                        HANDLE,
                                        WriteFileBuffer,
                                        DWORD,
                                        LPDWORD,
                                        OVERLAPPED.ptr
    );

    /*
    BOOL WINAPI ReadFile(
      __in         HANDLE hFile,
      __out        LPVOID ReadFileBuffer,
      __in         DWORD nNumberOfBytesToRead,
      __out_opt    LPDWORD lpNumberOfBytesRead,
      __inout_opt  LPOVERLAPPED lpOverlapped
    );
    */
    libFunc.ReadFile = kernel32dll.declare("ReadFile",
                                        WinABI,
                                        BOOL,
                                        HANDLE,
                                        ReadFileBuffer,
                                        DWORD,
                                        LPDWORD,
                                        OVERLAPPED.ptr
    );

    /*
    BOOL WINAPI CloseHandle(
      __in  HANDLE hObject
    );
    */
    libFunc.CloseHandle = kernel32dll.declare("CloseHandle",
                                            WinABI,
                                            BOOL,
                                            HANDLE
    );
}


function writePipe(pipe, data) {
    var bytesWritten = DWORD(0);

    var pData = new WriteFileBuffer();

    var numChunks = Math.floor(data.length / BufferSize);
    for (var chunk = 0; chunk <= numChunks; chunk ++) {
        var numBytes = chunk < numChunks ? BufferSize : data.length - chunk * BufferSize;
        for (var i=0; i < numBytes; i++) {
            pData[i] = data.charCodeAt(chunk * BufferSize + i) % 256;
        }

      var r = libFunc.WriteFile(pipe, pData, numBytes, bytesWritten.address(), null);
      if (bytesWritten.value != numBytes)
          throw("error: wrote "+bytesWritten.value+" instead of "+numBytes+" bytes");
    }
    postMessage("wrote "+data.length+" bytes of data");
}

function readString(data, length, charset) {
    var string = '', bytes = [];
    for(var i = 0;i < length; i++) {
        if(data[i] == 0 && charset != "null") // stop on NULL character for non-binary data
           break;

        bytes.push(data[i]);
    }

    return bytes;
}

function readPipe(pipe, charset) {
    while (true) {
        var bytesRead = DWORD(0);
        var line = new ReadFileBuffer();
        var r = libFunc.ReadFile(pipe, line, BufferSize, bytesRead.address(), null);

        if (!r) {
            // stop if we get an error (such as EOF reached)
            postMessage({msg: "info", data: "ReadFile failed"});
            break;
        }

        if (bytesRead.value > 0) {
            var c = readString(line, bytesRead.value, charset);
            postMessage({msg: "data", data: c, count: c.length});
        }
        else {
            break;
        }
    }
    libFunc.CloseHandle(pipe);
    postMessage({msg: "done"});
    kernel32dll.close();
    close();
}

onmessage = function (event) {
    let pipePtr;
    switch (event.data.msg) {
    case "init":
        initLib(event.data.libc);
        break;
    case "write":
        // data contents:
        //   msg: 'write'
        //   data: the data (string) to write
        //   pipe: ptr to pipe
        pipePtr = HANDLE.ptr(event.data.pipe);
        writePipe(pipePtr.contents, event.data.data);
        postMessage("WriteOK");
        break;
    case "read":
        initLib(event.data.libc);
        pipePtr = HANDLE.ptr(event.data.pipe);
        readPipe(pipePtr.contents, event.data.charset);
        break;
    case "close":
        pipePtr = HANDLE.ptr(event.data.pipe);
        postMessage("closing stdin\n");

        if (libFunc.CloseHandle(pipePtr.contents)) {
            postMessage("ClosedOK");
        }
        else
            postMessage("Could not close stdin handle");
        break;
    case "stop":
        kernel32dll.close();
        close();
        break;
    default:
        throw("error: Unknown command"+event.data.msg+"\n");
    }
    return;
};
