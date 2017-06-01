/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* exported zoomContent */

// Load the shared-head file first.
/* import-globals-from ../shared-head.js */
Services.scriptloader.loadSubScript(
  'chrome://mochitests/content/browser/accessible/tests/browser/shared-head.js',
  this);

async function zoomContent(browser, zoom)
{
  return ContentTask.spawn(browser, zoom, _zoom => {
    let docShell = content
      .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
      .getInterface(Components.interfaces.nsIWebNavigation)
      .QueryInterface(Components.interfaces.nsIDocShell);
    let docViewer = docShell.contentViewer;

    docViewer.fullZoom = _zoom;
  });
}

// Loading and common.js from accessible/tests/mochitest/ for all tests, as
// well as events.js.
loadScripts({ name: 'common.js', dir: MOCHITESTS_DIR }, 'events.js');
