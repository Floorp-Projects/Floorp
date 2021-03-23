/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function ContentSearchHandoffUIController() {
  this._isPrivateWindow = false;
  this._engineIcon = null;

  window.addEventListener("ContentSearchService", this);
  this._sendMsg("GetEngine");
}

ContentSearchHandoffUIController.prototype = {
  handleEvent(event) {
    let methodName = "_onMsg" + event.detail.type;
    if (methodName in this) {
      this[methodName](event.detail.data);
    }
  },

  get defaultEngine() {
    return this._defaultEngine;
  },

  _onMsgEngine({ isPrivateWindow, engine }) {
    this._isPrivateWindow = isPrivateWindow;
    this._updateEngine(engine);
  },

  _onMsgCurrentEngine(engine) {
    if (!this._isPrivateWindow) {
      this._updateEngine(engine);
    }
  },

  _onMsgCurrentPrivateEngine(engine) {
    if (this._isPrivateWindow) {
      this._updateEngine(engine);
    }
  },

  _updateEngine(engine) {
    this._defaultEngine = engine;
    if (this._engineIcon) {
      URL.revokeObjectURL(this._engineIcon);
    }

    // We only show the engines icon for app provided engines, otherwise show
    // a default. xref https://bugzilla.mozilla.org/show_bug.cgi?id=1449338#c19
    if (!engine.isAppProvided) {
      this._engineIcon = "chrome://global/skin/icons/search-glass.svg";
    } else if (engine.iconData) {
      this._engineIcon = this._getFaviconURIFromIconData(engine.iconData);
    } else {
      this._engineIcon = "chrome://global/skin/icons/defaultFavicon.svg";
    }

    document.body.style.setProperty(
      "--newtab-search-icon",
      "url(" + this._engineIcon + ")"
    );

    let fakeButton = document.querySelector(".search-handoff-button");
    let fakeInput = document.querySelector(".fake-textbox");
    if (!engine.isAppProvided) {
      document.l10n.setAttributes(
        fakeButton,
        this._isPrivateWindow
          ? "about-private-browsing-handoff-no-engine"
          : "newtab-search-box-handoff-input-no-engine"
      );
      document.l10n.setAttributes(
        fakeInput,
        this._isPrivateWindow
          ? "about-private-browsing-handoff-text-no-engine"
          : "newtab-search-box-handoff-text-no-engine"
      );
    } else {
      document.l10n.setAttributes(
        fakeButton,
        this._isPrivateWindow
          ? "about-private-browsing-handoff"
          : "newtab-search-box-handoff-input",
        {
          engine: engine.name,
        }
      );
      document.l10n.setAttributes(
        fakeInput,
        this._isPrivateWindow
          ? "about-private-browsing-handoff-text"
          : "newtab-search-box-handoff-text",
        {
          engine: engine.name,
        }
      );
    }
  },

  // If the favicon is an array buffer, convert it into a Blob URI.
  // Otherwise just return the plain URI.
  _getFaviconURIFromIconData(data) {
    if (typeof data === "string") {
      return data;
    }

    // If typeof(data) != "string", we assume it's an ArrayBuffer
    let blob = new Blob([data]);
    return URL.createObjectURL(blob);
  },

  _sendMsg(type, data = null) {
    dispatchEvent(
      new CustomEvent("ContentSearchClient", {
        detail: {
          type,
          data,
        },
      })
    );
  },
};
