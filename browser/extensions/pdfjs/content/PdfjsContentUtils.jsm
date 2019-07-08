/* Copyright 2012 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

"use strict";

var EXPORTED_SYMBOLS = ["PdfjsContentUtils"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var PdfjsContentUtils = {
  _mm: Services.cpmm,

  /*
   * Public API
   */

  /*
   * prefs utilities - the child does not have write access to prefs.
   * note, the pref names here are cross-checked against a list of
   * approved pdfjs prefs in chrome utils.
   */

  clearUserPref(aPrefName) {
    this._mm.sendSyncMessage("PDFJS:Parent:clearUserPref", {
      name: aPrefName,
    });
  },

  setIntPref(aPrefName, aPrefValue) {
    this._mm.sendSyncMessage("PDFJS:Parent:setIntPref", {
      name: aPrefName,
      value: aPrefValue,
    });
  },

  setBoolPref(aPrefName, aPrefValue) {
    this._mm.sendSyncMessage("PDFJS:Parent:setBoolPref", {
      name: aPrefName,
      value: aPrefValue,
    });
  },

  setCharPref(aPrefName, aPrefValue) {
    this._mm.sendSyncMessage("PDFJS:Parent:setCharPref", {
      name: aPrefName,
      value: aPrefValue,
    });
  },

  setStringPref(aPrefName, aPrefValue) {
    this._mm.sendSyncMessage("PDFJS:Parent:setStringPref", {
      name: aPrefName,
      value: aPrefValue,
    });
  },

  /*
   * Request the display of a notification warning in the associated window
   * when the renderer isn't sure a pdf displayed correctly.
   */
  displayWarning(aWindow, aMessage, aLabel, aAccessKey) {
    // the child's dom frame mm associated with the window.
    let winmm = aWindow.docShell.messageManager;
    winmm.sendAsyncMessage("PDFJS:Parent:displayWarning", {
      message: aMessage,
      label: aLabel,
      accessKey: aAccessKey,
    });
  },
};
