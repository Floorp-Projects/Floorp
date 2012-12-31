# -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

let WebrtcIndicator = {
  init: function () {
    let temp = {};
    Cu.import("resource:///modules/webrtcUI.jsm", temp);
    this.UIModule = temp.webrtcUI;

    this.updateButton();
  },

  get button() {
    delete this.button;
    return this.button = document.getElementById("webrtc-status-button");
  },

  updateButton: function () {
    this.button.hidden = !this.UIModule.showGlobalIndicator;
  },

  fillPopup: function (aPopup) {
    this._menuitemData = new WeakMap;
    for (let streamData of this.UIModule.activeStreams) {
      let menuitem = document.createElement("menuitem");
      menuitem.setAttribute("label", streamData.uri);
      menuitem.setAttribute("tooltiptext", streamData.uri);

      this._menuitemData.set(menuitem, streamData);

      aPopup.appendChild(menuitem);
    }
  },

  clearPopup: function (aPopup) {
    while (aPopup.lastChild)
      aPopup.removeChild(aPopup.lastChild);
  },

  menuCommand: function (aMenuitem) {
    let streamData = this._menuitemData.get(aMenuitem);
    if (!streamData)
      return;

    let tab = streamData.tab;
    let browserWindow = tab.ownerDocument.defaultView;
    browserWindow.gBrowser.selectedTab = tab;
    browserWindow.focus();
  }
}
