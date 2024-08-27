/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Implements nsIGeolocationUIUtilsWin
 *
 * @class GeolocationUIUtilsWin
 */
export class GeolocationUIUtilsWin {
  dismissPrompts(aBrowsingContext) {
    // browser will be null if the tab was closed
    let embedder = aBrowsingContext?.top.embedderElement;
    let owner = embedder?.ownerGlobal;
    if (owner) {
      let dialogBox = owner.gBrowser.getTabDialogBox(embedder);
      // Don't close any content-modal dialogs, because we could be doing
      // content analysis on something like a prompt() call.
      dialogBox.getTabDialogManager().abortDialogs();
    }
  }
}

GeolocationUIUtilsWin.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIGeolocationUIUtilsWin",
]);
