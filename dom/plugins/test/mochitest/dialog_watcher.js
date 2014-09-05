const EVENT_OBJECT_SHOW = 0x8002;
const EVENT_OBJECT_HIDE = 0x8003;
const WINEVENT_OUTOFCONTEXT = 0;
const WINEVENT_SKIPOWNPROCESS = 2;
const QS_ALLINPUT = 0x04FF;
const INFINITE = 0xFFFFFFFF;
const WAIT_OBJECT_0 = 0;
const WAIT_TIMEOUT = 258;
const PM_NOREMOVE = 0;

function DialogWatcher(titleText, onDialogStart, onDialogEnd) {
  this.titleText = titleText;
  this.onDialogStart = onDialogStart;
  this.onDialogEnd = onDialogEnd;
}

DialogWatcher.prototype.init = function() {
  this.hwnd = undefined;
  if (!this.user32) {
    this.user32 = ctypes.open("user32.dll");
  }
  if (!this.findWindow) {
    this.findWindow = user32.declare("FindWindowW",
                                     ctypes.winapi_abi,
                                     ctypes.uintptr_t,
                                     ctypes.jschar.ptr,
                                     ctypes.jschar.ptr);
  }
  if (!this.winEventProcType) {
    this.winEventProcType = ctypes.FunctionType(ctypes.stdcall_abi,
                                                ctypes.void_t,
                                                [ctypes.uintptr_t,
                                                ctypes.uint32_t,
                                                ctypes.uintptr_t,
                                                ctypes.long,
                                                ctypes.long,
                                                ctypes.uint32_t,
                                                ctypes.uint32_t]).ptr;
  }
  if (!this.setWinEventHook) {
    this.setWinEventHook = user32.declare("SetWinEventHook",
                                          ctypes.winapi_abi,
                                          ctypes.uintptr_t,
                                          ctypes.uint32_t,
                                          ctypes.uint32_t,
                                          ctypes.uintptr_t,
                                          this.winEventProcType,
                                          ctypes.uint32_t,
                                          ctypes.uint32_t,
                                          ctypes.uint32_t);
  }
  if (!this.unhookWinEvent) {
    this.unhookWinEvent = user32.declare("UnhookWinEvent",
                                         ctypes.winapi_abi,
                                         ctypes.int,
                                         ctypes.uintptr_t);
  }
  if (!this.pointType) {
    this.pointType = ctypes.StructType("tagPOINT",
                                       [ { "x": ctypes.long },
                                         { "y": ctypes.long } ] );
  }
  if (!this.msgType) {
    this.msgType = ctypes.StructType("tagMSG",
                                     [ { "hwnd": ctypes.uintptr_t },
                                       { "message": ctypes.uint32_t },
                                       { "wParam": ctypes.uintptr_t },
                                       { "lParam": ctypes.intptr_t },
                                       { "time": ctypes.uint32_t },
                                       { "pt": this.pointType } ] );
  }
  if (!this.peekMessage) {
    this.peekMessage = user32.declare("PeekMessageW",
                                      ctypes.winapi_abi,
                                      ctypes.int,
                                      this.msgType.ptr,
                                      ctypes.uintptr_t,
                                      ctypes.uint32_t,
                                      ctypes.uint32_t,
                                      ctypes.uint32_t);
  }
  if (!this.msgWaitForMultipleObjects) {
    this.msgWaitForMultipleObjects = user32.declare("MsgWaitForMultipleObjects",
                                                    ctypes.winapi_abi,
                                                    ctypes.uint32_t,
                                                    ctypes.uint32_t,
                                                    ctypes.uintptr_t.ptr,
                                                    ctypes.int,
                                                    ctypes.uint32_t,
                                                    ctypes.uint32_t);
  }
  if (!this.getWindowTextW) {
    this.getWindowTextW = user32.declare("GetWindowTextW",
                                         ctypes.winapi_abi,
                                         ctypes.int,
                                         ctypes.uintptr_t,
                                         ctypes.jschar.ptr,
                                         ctypes.int);
  }
  if (!this.messageBox) {
    // Handy for debugging this code
    this.messageBox = user32.declare("MessageBoxW",
                                     ctypes.winapi_abi,
                                     ctypes.int,
                                     ctypes.uintptr_t,
                                     ctypes.jschar.ptr,
                                     ctypes.jschar.ptr,
                                     ctypes.uint32_t);
  }
};

DialogWatcher.prototype.getWindowText = function(hwnd) {
  var bufType = ctypes.ArrayType(ctypes.jschar);
  var buffer = new bufType(256);

  if (this.getWindowTextW(hwnd, buffer, buffer.length)) {
    return buffer.readString();
  }
};

DialogWatcher.prototype.processWindowEvents = function(timeout) {
  var onWinEvent = function(self, hook, event, hwnd, idObject, idChild, dwEventThread, dwmsEventTime) {
    var nhwnd = Number(hwnd)
    if (event == EVENT_OBJECT_SHOW) {
      if (nhwnd == self.hwnd) {
        // We've already picked up this event via FindWindow
        return;
      }
      var windowText = self.getWindowText(hwnd);
      if (windowText == self.titleText && self.onDialogStart) {
        self.hwnd = nhwnd;
        self.onDialogStart(nhwnd);
      }
    } else if (event == EVENT_OBJECT_HIDE && nhwnd == self.hwnd && self.onDialogEnd) {
      self.onDialogEnd();
      self.hwnd = null;
    }
  };
  var self = this;
  var callback = this.winEventProcType(function(hook, event, hwnd, idObject,
                                                idChild, dwEventThread,
                                                dwmsEventTime) {
      onWinEvent(self, hook, event, hwnd, idObject, idChild, dwEventThread,
                 dwmsEventTime);
    } );
  var hook = this.setWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_HIDE,
                                  0, callback, 0, 0,
                                  WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
  if (!hook) {
    return;
  }
  // Check if the window is already showing
  var hwnd = this.findWindow(null, this.titleText);
  if (hwnd && hwnd > 0) {
    this.hwnd = Number(hwnd);
    if (this.onDialogStart) {
      this.onDialogStart(this.hwnd);
    }
  }

  if (!timeout) {
    timeout = INFINITE;
  }

  var waitStatus = WAIT_OBJECT_0;
  var expectingStart = this.onDialogStart && this.hwnd === undefined;
  var startWaitTime = Date.now();
  while (this.hwnd === undefined || this.onDialogEnd && this.hwnd) {
    waitStatus = this.msgWaitForMultipleObjects(0, null, 0, expectingStart ?
                                                            INFINITE : timeout, 0);
    if (waitStatus == WAIT_OBJECT_0) {
      var msg = new this.msgType;
      this.peekMessage(msg.address(), 0, 0, 0, PM_NOREMOVE);
    }
    if (waitStatus == WAIT_TIMEOUT || (Date.now() - startWaitTime) >= timeout) {
      break;
    }
  }

  this.unhookWinEvent(hook);
  // Returns true if the hook was successful, something was found, and we never timed out
  return this.hwnd !== undefined && waitStatus == WAIT_OBJECT_0;
};

