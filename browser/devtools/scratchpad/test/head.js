/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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
