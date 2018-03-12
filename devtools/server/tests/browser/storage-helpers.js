/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* exported openTabAndSetupStorage, clearStorage */

"use strict";

/**
 * This generator function opens the given url in a new tab, then sets up the
 * page by waiting for all cookies, indexedDB items etc. to be created.
 *
 * @param url {String} The url to be opened in the new tab
 *
 * @return {Promise} A promise that resolves after storage inspector is ready
 */
async function openTabAndSetupStorage(url) {
  let content = await addTab(url);

  // Setup the async storages in main window and for all its iframes
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    /**
     * Get all windows including frames recursively.
     *
     * @param {Window} [baseWindow]
     *        The base window at which to start looking for child windows
     *        (optional).
     * @return {Set}
     *         A set of windows.
     */
    function getAllWindows(baseWindow) {
      let windows = new Set();

      let _getAllWindows = function(win) {
        windows.add(win.wrappedJSObject);

        for (let i = 0; i < win.length; i++) {
          _getAllWindows(win[i]);
        }
      };
      _getAllWindows(baseWindow);

      return windows;
    }

    let windows = getAllWindows(content);
    for (let win of windows) {
      if (win.setup) {
        await win.setup();
      }
    }
  });
}

async function clearStorage() {
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    /**
     * Get all windows including frames recursively.
     *
     * @param {Window} [baseWindow]
     *        The base window at which to start looking for child windows
     *        (optional).
     * @return {Set}
     *         A set of windows.
     */
    function getAllWindows(baseWindow) {
      let windows = new Set();

      let _getAllWindows = function(win) {
        windows.add(win.wrappedJSObject);

        for (let i = 0; i < win.length; i++) {
          _getAllWindows(win[i]);
        }
      };
      _getAllWindows(baseWindow);

      return windows;
    }

    let windows = getAllWindows(content);
    for (let win of windows) {
      if (win.clear) {
        await win.clear();
      }
    }
  });
}
