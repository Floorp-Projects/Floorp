# -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
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
      let pageURI = Services.io.newURI(streamData.uri, null, null);
      let menuitem = document.createElement("menuitem");
      menuitem.setAttribute("class", "menuitem-iconic");
      menuitem.setAttribute("label", streamData.browser.contentTitle || streamData.uri);
      menuitem.setAttribute("tooltiptext", streamData.uri);
      PlacesUtils.favicons.getFaviconURLForPage(pageURI, function (aURI) {
        if (aURI) {
          let iconURL = PlacesUtils.favicons.getFaviconLinkForIcon(aURI).spec;
          menuitem.setAttribute("image", iconURL);
        }
      });

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

    let browserWindow = streamData.browser.ownerDocument.defaultView;
    if (streamData.tab) {
      browserWindow.gBrowser.selectedTab = streamData.tab;
    } else {
      streamData.browser.focus();
    }
    browserWindow.focus();
    PopupNotifications.getNotification("webRTC-sharingDevices",
                                       streamData.browser).reshow();
  }
}
