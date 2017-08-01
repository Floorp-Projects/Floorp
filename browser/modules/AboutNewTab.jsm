/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

this.EXPORTED_SYMBOLS = [ "AboutNewTab" ];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AutoMigrate",
  "resource:///modules/AutoMigrate.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RemotePages",
  "resource://gre/modules/RemotePageManager.jsm");

var AboutNewTab = {

  pageListener: null,

  isOverridden: false,

  init(pageListener) {
    if (this.isOverridden) {
      return;
    }
    this.pageListener = pageListener || new RemotePages("about:newtab");
    this.pageListener.addMessageListener("NewTab:Customize", this.customize);
    this.pageListener.addMessageListener("NewTab:MaybeShowMigrateMessage",
      this.maybeShowMigrateMessage);
  },

  maybeShowMigrateMessage({ target }) {
    AutoMigrate.shouldShowMigratePrompt(target.browser).then((prompt) => {
      if (prompt) {
        AutoMigrate.showUndoNotificationBar(target.browser);
      }
    });
  },

  customize(message) {
    NewTabUtils.allPages.enabled = message.data.enabled;
    NewTabUtils.allPages.enhanced = message.data.enhanced;
  },

  uninit() {
    if (this.pageListener) {
      this.pageListener.destroy();
      this.pageListener = null;
    }
  },

  override(shouldPassPageListener) {
    this.isOverridden = true;
    const pageListener = this.pageListener;
    if (!pageListener) {
      return;
    }
    if (shouldPassPageListener) {
      this.pageListener = null;
      pageListener.removeMessageListener("NewTab:Customize", this.customize);
      pageListener.removeMessageListener("NewTab:MaybeShowMigrateMessage",
        this.maybeShowMigrateMessage);
      return pageListener;
    }
    this.uninit();
  },

  reset(pageListener) {
    this.isOverridden = false;
    this.init(pageListener);
  }
};
