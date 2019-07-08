/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutPrivateBrowsingHandler"];

const { RemotePages } = ChromeUtils.import(
  "resource://gre/modules/remotepagemanager/RemotePageManagerParent.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var AboutPrivateBrowsingHandler = {
  _inited: false,
  _topics: ["OpenPrivateWindow", "SearchHandoff"],

  init() {
    this.pageListener = new RemotePages("about:privatebrowsing");
    for (let topic of this._topics) {
      this.pageListener.addMessageListener(
        topic,
        this.receiveMessage.bind(this)
      );
    }
    this._inited = true;
  },

  uninit() {
    if (!this._inited) {
      return;
    }
    for (let topic of this._topics) {
      this.pageListener.removeMessageListener(topic);
    }
    this.pageListener.destroy();
  },

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "OpenPrivateWindow": {
        let win = aMessage.target.browser.ownerGlobal;
        win.OpenBrowserWindow({ private: true });
        break;
      }
      case "SearchHandoff": {
        let searchAlias = "";
        let searchAliases =
          Services.search.defaultEngine.wrappedJSObject.__internalAliases;
        if (searchAliases && searchAliases.length > 0) {
          searchAlias = `${searchAliases[0]} `;
        }
        let urlBar = aMessage.target.browser.ownerGlobal.gURLBar;
        let isFirstChange = true;

        if (!aMessage.data || !aMessage.data.text) {
          urlBar.setHiddenFocus();
        } else {
          // Pass the provided text to the awesomebar. Prepend the @engine shortcut.
          urlBar.search(`${searchAlias}${aMessage.data.text}`);
          isFirstChange = false;
        }

        let checkFirstChange = () => {
          // Check if this is the first change since we hidden focused. If it is,
          // remove hidden focus styles, prepend the search alias and hide the
          // in-content search.
          if (isFirstChange) {
            isFirstChange = false;
            urlBar.removeHiddenFocus();
            urlBar.search(searchAlias);
            aMessage.target.sendAsyncMessage("HideSearch");
            urlBar.removeEventListener("compositionstart", checkFirstChange);
            urlBar.removeEventListener("paste", checkFirstChange);
          }
        };

        let onKeydown = ev => {
          // Check if the keydown will cause a value change.
          if (ev.key.length === 1 && !ev.altKey && !ev.ctrlKey && !ev.metaKey) {
            checkFirstChange();
          }
          // If the Esc button is pressed, we are done. Show in-content search and cleanup.
          if (ev.key === "Escape") {
            onDone();
          }
        };

        let onDone = () => {
          // We are done. Show in-content search again and cleanup.
          aMessage.target.sendAsyncMessage("ShowSearch");
          urlBar.removeHiddenFocus();

          urlBar.removeEventListener("keydown", onKeydown);
          urlBar.removeEventListener("mousedown", onDone);
          urlBar.removeEventListener("blur", onDone);
          urlBar.removeEventListener("compositionstart", checkFirstChange);
          urlBar.removeEventListener("paste", checkFirstChange);
        };

        urlBar.addEventListener("keydown", onKeydown);
        urlBar.addEventListener("mousedown", onDone);
        urlBar.addEventListener("blur", onDone);
        urlBar.addEventListener("compositionstart", checkFirstChange);
        urlBar.addEventListener("paste", checkFirstChange);
        break;
      }
    }
  },
};
