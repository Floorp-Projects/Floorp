/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [ "AboutNewTab" ];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "AutoMigrate",
  "resource:///modules/AutoMigrate.jsm");
ChromeUtils.defineModuleGetter(this, "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm");
ChromeUtils.defineModuleGetter(this, "RemotePages",
  "resource://gre/modules/RemotePageManager.jsm");

var AboutNewTab = {

  pageListener: null,

  isOverridden: false,

  init(pageListener) {
    if (this.isOverridden) {
      return;
    }
    this.pageListener = pageListener || new RemotePages(["about:home", "about:newtab", "about:welcome"]);
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
    if (!pageListener)
      return null;
    if (shouldPassPageListener) {
      this.pageListener = null;
      pageListener.removeMessageListener("NewTab:Customize", this.customize);
      pageListener.removeMessageListener("NewTab:MaybeShowMigrateMessage",
        this.maybeShowMigrateMessage);
      return pageListener;
    }
    this.uninit();
    return null;
  },

  reset(pageListener) {
    this.isOverridden = false;
    this.init(pageListener);
  }
};
