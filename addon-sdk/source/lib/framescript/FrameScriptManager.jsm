/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const globalMM = Components.classes["@mozilla.org/globalmessagemanager;1"].
                 getService(Components.interfaces.nsIMessageListenerManager);

// Load frame scripts from the same dir as this module.
// Since this JSM will be loaded using require(), PATH will be
// overridden while running tests, just like any other module.
const PATH = __URI__.replace('framescript/FrameScriptManager.jsm', '');

// Builds a unique loader ID for this runtime. We prefix with the SDK path so
// overriden versions of the SDK don't conflict
var LOADER_ID = 0;
this.getNewLoaderID = () => {
  return PATH + ":" + LOADER_ID++;
}

const frame_script = function(contentFrame, PATH) {
  let { registerContentFrame } = Components.utils.import(PATH + 'framescript/content.jsm', {});
  registerContentFrame(contentFrame);
}
globalMM.loadFrameScript("data:,(" + frame_script.toString() + ")(this, " + JSON.stringify(PATH) + ");", true);

this.EXPORTED_SYMBOLS = ['getNewLoaderID'];
