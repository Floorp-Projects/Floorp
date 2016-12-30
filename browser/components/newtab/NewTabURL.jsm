/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals XPCOMUtils, aboutNewTabService*/
/* exported NewTabURL */

"use strict";

const {utils: Cu} = Components;

this.EXPORTED_SYMBOLS = ["NewTabURL"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

this.NewTabURL = {

  get() {
    return aboutNewTabService.newTabURL;
  },

  get overridden() {
    return aboutNewTabService.overridden;
  },

  override(newURL) {
    aboutNewTabService.newTabURL = newURL;
  },

  reset() {
    aboutNewTabService.resetNewTabURL();
  }
};
