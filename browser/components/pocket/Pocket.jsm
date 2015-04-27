/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

this.EXPORTED_SYMBOLS = ["Pocket"];

Cu.import("resource://gre/modules/Http.jsm");
Cu.import("resource://gre/modules/Services.jsm");

let Pocket = {
  get isLoggedIn() {
    return !!this._accessToken;
  },

  prefBranch: Services.prefs.getBranch("browser.pocket.settings."),

  get hostname() Services.prefs.getCharPref("browser.pocket.hostname"),

  get _accessToken() {
    let sessionId, accessToken;
    let cookies = Services.cookies.getCookiesFromHost(this.hostname);
    while (cookies.hasMoreElements()) {
      let cookie = cookies.getNext().QueryInterface(Ci.nsICookie2);
      if (cookie.name == "ftv1")
        accessToken = cookie.value;
      else if (cookie.name == "fsv1")
        sessionId = cookie.value;
    }

    if (!accessToken)
      return null;

    let lastSessionId;
    try {
      lastSessionId = this.prefBranch.getCharPref("sessionId");
    } catch (e) { }
    if (sessionId != lastSessionId)
      this.prefBranch.deleteBranch("");
    this.prefBranch.setCharPref("sessionId", sessionId);

    return accessToken;
  },

  save(url, title) {
    let since = "0";
    try {
      since = this.prefBranch.getCharPref("latestSince");
    } catch (e) { }

    let data = {url: url, since: since, title: title};

    return new Promise((resolve, reject) => {
      this._send("firefox/save", data,
        data => {
          this.prefBranch.setCharPref("latestSince", data.since);
          resolve(data.item);
        },
        error => { reject(error); }
      );
    });
  },

  remove(itemId) {
    let actions = [{ action: "delete", item_id: itemId }];
    this._send("send", {actions: JSON.stringify(actions)});
  },

  tag(itemId, tags) {
    let actions = [{ action: "tags_add", item_id: itemId, tags: tags }];
    this._send("send", {actions: JSON.stringify(actions)});
  },

  _send(url, data, onSuccess, onError) {
    let token = this._accessToken;
    if (!token)
      throw "Attempted to send a request to Pocket while not logged in";

    let browserLocale = Cc["@mozilla.org/chrome/chrome-registry;1"]
                          .getService(Ci.nsIXULChromeRegistry)
                          .getSelectedLocale("browser");

    let postData = [
      ["access_token", token],
      ["consumer_key", "40249-e88c401e1b1f2242d9e441c4"],
      ["locale_lang", browserLocale]
    ];

    for (let key in data)
      postData.push([key, data[key]]);

    httpRequest("https://" + this.hostname + "/v3/" + url, {
      headers: [["X-Accept", "application/json"]],
      postData: postData,
      onLoad: (responseText) => {
        if (onSuccess)
          onSuccess(JSON.parse(responseText));
      },
      onError: function(error, responseText, xhr) {
        if (!onError)
          return;
        let errorMessage = xhr.getResponseHeader("X-Error");
        onError(new Error(error + " - " + errorMessage));
      }
    });
  }
};
