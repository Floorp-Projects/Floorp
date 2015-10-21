/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

this.EXPORTED_SYMBOLS = [ "NewTabURL" ];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");
XPCOMUtils.defineLazyModuleGetter(this, "Deprecated",
                                  "resource://gre/modules/Deprecated.jsm");

const DepecationURL = "https://bugzilla.mozilla.org/show_bug.cgi?id=1204983#c89";

this.NewTabURL = {

  get: function() {
    Deprecated.warning("NewTabURL.get is deprecated, please query aboutNewTabService.newTabURL", DepecationURL);
    return aboutNewTabService.newTabURL;
  },

  get overridden() {
    Deprecated.warning("NewTabURL.overridden is deprecated, please query aboutNewTabService.overridden", DepecationURL);
    return aboutNewTabService.overridden;
  },

  override: function(newURL) {
    Deprecated.warning("NewTabURL.override is deprecated, please set aboutNewTabService.newTabURL", DepecationURL);
    aboutNewTabService.newTabURL = newURL;
  },

  reset: function() {
    Deprecated.warning("NewTabURL.reset is deprecated, please use aboutNewTabService.resetNewTabURL()", DepecationURL);
    aboutNewTabService.resetNewTabURL();
  }
};
