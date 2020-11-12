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

  _onMsgEngine({ isPrivateWindow, engine }) {
    this._isPrivateWindow = isPrivateWindow;
    this._updateEngineIcon(engine);
  },

  _onMsgCurrentEngine(engine) {
    if (!this._isPrivateWindow) {
      this._updateEngineIcon(engine);
    }
  },

  _onMsgCurrentPrivateEngine(engine) {
    if (this._isPrivateWindow) {
      this._updateEngineIcon(engine);
    }
  },

  _updateEngineIcon(engine) {
    if (this._engineIcon) {
      URL.revokeObjectURL(this._engineIcon);
    }

    if (engine.iconData) {
      this._engineIcon = this._getFaviconURIFromIconData(engine.iconData);
    } else {
      this._engineIcon = "chrome://mozapps/skin/places/defaultFavicon.svg";
    }

    document.body.style.setProperty(
      "--newtab-search-icon",
      "url(" + this._engineIcon + ")"
    );
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
