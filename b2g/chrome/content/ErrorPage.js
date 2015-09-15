/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

var Cu = Components.utils;
var Cc = Components.classes;
var Ci = Components.interfaces;

dump("############ ErrorPage.js\n");

var ErrorPageHandler = {
  _reload: function() {
    docShell.QueryInterface(Ci.nsIWebNavigation).reload(Ci.nsIWebNavigation.LOAD_FLAGS_NONE);
  },

  _certErrorPageEventHandler: function(e) {
    let target = e.originalTarget;
    let errorDoc = target.ownerDocument;

    // If the event came from an ssl error page, it is one of the "Add
    // Exception…" buttons.
    if (/^about:certerror\?e=nssBadCert/.test(errorDoc.documentURI)) {
      let permanent = errorDoc.getElementById("permanentExceptionButton");
      let temp = errorDoc.getElementById("temporaryExceptionButton");
      if (target == temp || target == permanent) {
        sendAsyncMessage("ErrorPage:AddCertException", {
          url: errorDoc.location.href,
          isPermanent: target == permanent
        });
      }
    }
  },

  _bindPageEvent: function(target) {
    if (!target) {
      return;
    }

    if (/^about:certerror/.test(target.documentURI)) {
      let errorPageEventHandler = this._certErrorPageEventHandler.bind(this);
      addEventListener("click", errorPageEventHandler, true, false);
      let listener = function() {
        removeEventListener("click", errorPageEventHandler, true);
        removeEventListener("pagehide", listener, true);
      }.bind(this);

      addEventListener("pagehide", listener, true);
    }
  },

  domContentLoadedHandler: function(e) {
    let target = e.originalTarget;
    let targetDocShell = target.defaultView
                               .QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIWebNavigation);
    if (targetDocShell != docShell) {
      return;
    }
    this._bindPageEvent(target);
  },

  init: function() {
    addMessageListener("ErrorPage:ReloadPage", this._reload.bind(this));
    addEventListener('DOMContentLoaded',
                     this.domContentLoadedHandler.bind(this),
                     true);
    this._bindPageEvent(content.document);
  }
};

ErrorPageHandler.init();
