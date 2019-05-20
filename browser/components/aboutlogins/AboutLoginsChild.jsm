/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutLoginsChild"];

const {ActorChild} = ChromeUtils.import("resource://gre/modules/ActorChild.jsm");
const {LoginHelper} = ChromeUtils.import("resource://gre/modules/LoginHelper.jsm");
ChromeUtils.defineModuleGetter(this, "AppConstants",
                               "resource://gre/modules/AppConstants.jsm");

class AboutLoginsChild extends ActorChild {
  handleEvent(event) {
    switch (event.type) {
      case "AboutLoginsInit": {
        this.mm.sendAsyncMessage("AboutLogins:Subscribe");

        let documentElement = this.content.document.documentElement;
        documentElement.classList.toggle("official-branding", AppConstants.MOZILLA_OFFICIAL);

        let waivedContent = Cu.waiveXrays(this.content);
        let AboutLoginsUtils = {
          doLoginsMatch(loginA, loginB) {
            return LoginHelper.doLoginsMatch(loginA, loginB, {});
          },
        };
        waivedContent.AboutLoginsUtils = Cu.cloneInto(AboutLoginsUtils, waivedContent, {
          cloneFunctions: true,
        });
        break;
      }
      case "AboutLoginsDeleteLogin": {
        this.mm.sendAsyncMessage("AboutLogins:DeleteLogin", {login: event.detail});
        break;
      }
      case "AboutLoginsOpenSite": {
        this.mm.sendAsyncMessage("AboutLogins:OpenSite", {login: event.detail});
        break;
      }
      case "AboutLoginsUpdateLogin": {
        this.mm.sendAsyncMessage("AboutLogins:UpdateLogin", {login: event.detail});
        break;
      }
    }
  }

  receiveMessage(message) {
    switch (message.name) {
      case "AboutLogins:AllLogins":
        this.sendToContent("AllLogins", message.data);
        break;
      case "AboutLogins:LoginAdded":
        this.sendToContent("LoginAdded", message.data);
        break;
      case "AboutLogins:LoginModified":
        this.sendToContent("LoginModified", message.data);
        break;
      case "AboutLogins:LoginRemoved":
        this.sendToContent("LoginRemoved", message.data);
        break;
    }
  }

  sendToContent(messageType, detail) {
    let message = Object.assign({messageType}, {value: detail});
    let event = new this.content.CustomEvent("AboutLoginsChromeToContent", {
      detail: Cu.cloneInto(message, this.content),
    });
    this.content.dispatchEvent(event);
  }
}
