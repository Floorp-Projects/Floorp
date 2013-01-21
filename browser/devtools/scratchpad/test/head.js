/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let tempScope = {};

Cu.import("resource://gre/modules/NetUtil.jsm", tempScope);
Cu.import("resource://gre/modules/FileUtils.jsm", tempScope);

let NetUtil = tempScope.NetUtil;
let FileUtils = tempScope.FileUtils;

let gScratchpadWindow; // Reference to the Scratchpad chrome window object

/**
 * Open a Scratchpad window.
 *
 * @param function aReadyCallback
 *        Optional. The function you want invoked when the Scratchpad instance
 *        is ready.
 * @param object aOptions
 *        Optional. Options for opening the scratchpad:
 *        - window
 *          Provide this if there's already a Scratchpad window you want to wait
 *          loading for.
 *        - state
 *          Scratchpad state object. This is used when Scratchpad is open.
 *        - noFocus
 *          Boolean that tells you do not want the opened window to receive
 *          focus.
 * @return nsIDOMWindow
 *         The new window object that holds Scratchpad. Note that the
 *         gScratchpadWindow global is also updated to reference the new window
 *         object.
 */
function openScratchpad(aReadyCallback, aOptions)
{
  aOptions = aOptions || {};

  let win = aOptions.window ||
            Scratchpad.ScratchpadManager.openScratchpad(aOptions.state);
  if (!win) {
    return;
  }

  let onLoad = function() {
    win.removeEventListener("load", onLoad, false);

    win.Scratchpad.addObserver({
      onReady: function(aScratchpad) {
        aScratchpad.removeObserver(this);

        if (aOptions.noFocus) {
          aReadyCallback(win, aScratchpad);
        } else {
          waitForFocus(aReadyCallback.bind(null, win, aScratchpad), win);
        }
      }
    });
  };

  if (aReadyCallback) {
    win.addEventListener("load", onLoad, false);
  }

  gScratchpadWindow = win;
  return gScratchpadWindow;
}

/**
 * Create a temporary file, write to it and call a callback
 * when done.
 *
 * @param string aName
 *        Name of your temporary file.
 * @param string aContent
 *        Temporary file's contents.
 * @param function aCallback
 *        Optional callback to be called when we're done writing
 *        to the file. It will receive two parameters: status code
 *        and a file object.
 */
function createTempFile(aName, aContent, aCallback=function(){})
{
  // Create a temporary file.
  let file = FileUtils.getFile("TmpD", [aName]);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));

  // Write the temporary file.
  let fout = Cc["@mozilla.org/network/file-output-stream;1"].
             createInstance(Ci.nsIFileOutputStream);
  fout.init(file.QueryInterface(Ci.nsILocalFile), 0x02 | 0x08 | 0x20,
            parseInt("644", 8), fout.DEFER_OPEN);

  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                  createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  let fileContentStream = converter.convertToInputStream(aContent);

  NetUtil.asyncCopy(fileContentStream, fout, function (aStatus) {
    aCallback(aStatus, file);
  });
}

function cleanup()
{
  if (gScratchpadWindow) {
    gScratchpadWindow.close();
    gScratchpadWindow = null;
  }
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
}

registerCleanupFunction(cleanup);
