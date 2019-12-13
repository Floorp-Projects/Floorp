/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This file assumes head.js is loaded in the global scope.
/* import-globals-from head.js */

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
  const content = await addTab(url);

  // Setup the async storages in main window and for all its iframes
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
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
      const windows = new Set();

      const _getAllWindows = function(win) {
        windows.add(win.wrappedJSObject);

        for (let i = 0; i < win.length; i++) {
          _getAllWindows(win[i]);
        }
      };
      _getAllWindows(baseWindow);

      return windows;
    }

    const windows = getAllWindows(content);
    for (const win of windows) {
      if (win.setup) {
        await win.setup();
      }
    }
  });
  // selected tab is set in addTab
  const target = await getTargetForTab(gBrowser.selectedTab);
  const front = await target.getFront("storage");
  return { target, front };
}

async function clearStorage() {
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
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
      const windows = new Set();

      const _getAllWindows = function(win) {
        windows.add(win.wrappedJSObject);

        for (let i = 0; i < win.length; i++) {
          _getAllWindows(win[i]);
        }
      };
      _getAllWindows(baseWindow);

      return windows;
    }

    const windows = getAllWindows(content);
    for (const win of windows) {
      if (win.clear) {
        await win.clear();
      }
    }
  });
}
