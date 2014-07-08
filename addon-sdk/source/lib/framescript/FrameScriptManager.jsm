/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const globalMM = Components.classes["@mozilla.org/globalmessagemanager;1"].
                 getService(Components.interfaces.nsIMessageListenerManager);

// Load frame scripts from the same dir as this module.
// Since this JSM will be loaded using require(), PATH will be
// overridden while running tests, just like any other module.
const PATH = __URI__.replace('FrameScriptManager.jsm', '');

// ensure frame scripts are loaded only once
let loadedTabEvents = false;

function enableTabEvents() {
  if (loadedTabEvents) 
    return;

  loadedTabEvents = true;
  globalMM.loadFrameScript(PATH + 'tab-events.js', true);
}

const EXPORTED_SYMBOLS = ['enableTabEvents'];
