/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

// The panel module currently supports only Firefox.
// See: https://bugzilla.mozilla.org/show_bug.cgi?id=jetpack-panel-apps
module.metadata = {
  'stability': 'unstable',
  'engines': {
    'Firefox': '*'
  }
};

const { getMostRecentBrowserWindow, windows: getWindows } = require('../window/utils');
const { ignoreWindow } = require('../private-browsing/utils');
const { isPrivateBrowsingSupported } = require('../self');

function getWindow(anchor) {
  let window;
  let windows = getWindows("navigator:browser", {
    includePrivate: isPrivateBrowsingSupported
  });

  if (anchor) {
    let anchorWindow = anchor.ownerDocument.defaultView.top;
    let anchorDocument = anchorWindow.document;

    // loop thru supported windows
    for (let enumWindow of windows) {
      // Check if the anchor is in this browser window.
      if (enumWindow == anchorWindow) {
        window = anchorWindow;
        break;
      }

      // Check if the anchor is in a browser tab in this browser window.
      try {
        let browser = enumWindow.gBrowser.getBrowserForDocument(anchorDocument);
        if (browser) {
          window = enumWindow;
          break;
        }
      }
      catch (e) {
      }

      // Look in other subdocuments (sidebar, etc.)?
    }
  }

  // If we didn't find the anchor's window (or we have no anchor),
  // return the most recent browser window.
  if (!window)
    window = getMostRecentBrowserWindow();

  // if the window is not supported, then it should be ignored
  if (ignoreWindow(window)) {
  	return null;
  }

  return window;
}
exports.getWindow = getWindow;
