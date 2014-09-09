var user32;
var sendMessage;
var getDlgItem;
var messageBox;
var watcher;

importScripts("hangui_common.js");
importScripts("dialog_watcher.js");

function initCTypes() {
  if (!user32) {
    user32 = ctypes.open("user32.dll");
  }
  if (!getDlgItem) {
    getDlgItem = user32.declare("GetDlgItem",
                                ctypes.winapi_abi,
                                ctypes.uintptr_t,
                                ctypes.uintptr_t,
                                ctypes.int);
  }
  if (!sendMessage) {
    sendMessage = user32.declare("SendMessageW",
                                 ctypes.winapi_abi,
                                 ctypes.intptr_t,
                                 ctypes.uintptr_t,
                                 ctypes.uint32_t,
                                 ctypes.uintptr_t,
                                 ctypes.intptr_t);
  }
  if (!messageBox) {
    // Handy for debugging the test itself
    messageBox = user32.declare("MessageBoxW",
                                ctypes.winapi_abi,
                                ctypes.int,
                                ctypes.uintptr_t,
                                ctypes.char16_t.ptr,
                                ctypes.char16_t.ptr,
                                ctypes.uint32_t);
  }
  if (!watcher) {
    watcher = new DialogWatcher("Warning: Unresponsive plugin");
  }
}

function postSuccess(params) {
  self.postMessage({"status": true, "params": params});
}

function postFail(params, msg) {
  self.postMessage({"status": false, "params": params, "msg": msg});
}

function onDialogStart(inparams, hwnd) {
  var params = Object.create(inparams);
  params.testName += " (Start)";
  params.callback = null;
  if (!params.expectToFind) {
    postFail(params, "Dialog showed when we weren't expecting it to!");
    return;
  }
  if (params.opCode == HANGUIOP_CANCEL) {
    sendMessage(hwnd, WM_CLOSE, 0, 0);
  } else if (params.opCode == HANGUIOP_COMMAND) {
    if (params.check) {
      var checkbox = getDlgItem(hwnd, IDC_NOFUTURE);
      if (!checkbox) {
        postFail(params, "Couldn't find checkbox");
        return;
      }
      sendMessage(checkbox, BM_SETCHECK, BST_CHECKED, 0);
      sendMessage(hwnd, WM_COMMAND, (BN_CLICKED << 16) | IDC_NOFUTURE, checkbox);
    }
    var button = getDlgItem(hwnd, params.commandId);
    if (!button) {
      postFail(params,
               "GetDlgItem failed to find button with ID " + params.commandId);
      return;
    }
    sendMessage(hwnd, WM_COMMAND, (BN_CLICKED << 16) | params.commandId, button);
  }
  postSuccess(params);
}

function onDialogEnd(inparams) {
  var params = Object.create(inparams);
  params.testName += " (End)";
  params.callback = inparams.callback;
  postSuccess(params);
}

self.onmessage = function(event) {
  initCTypes();
  watcher.init();
  var params = event.data;
  var timeout = params.timeoutMs;
  if (params.expectToFind) {
    watcher.onDialogStart = function(hwnd) { onDialogStart(params, hwnd); };
    if (params.expectToClose) {
      watcher.onDialogEnd = function() { onDialogEnd(params); };
    }
  } else {
    watcher.onDialogStart = null;
    watcher.onDialogEnd = null;
  }
  var result = watcher.processWindowEvents(timeout);
  if (result === null) {
    postFail(params, "Hook failed");
  } else if (!result) {
    if (params.expectToFind) {
      postFail(params, "The dialog didn't show but we were expecting it to");
    } else {
      postSuccess(params);
    }
  }
}

self.onerror = function(event) {
  var msg = "Error: " + event.message + " at " + event.filename + ":" + event.lineno;
  postFail(null, msg);
};

