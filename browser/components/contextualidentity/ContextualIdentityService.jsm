/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["ContextualIdentityService"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm")

XPCOMUtils.defineLazyGetter(this, "gBrowserBundle", function() {
  return Services.strings.createBundle("chrome://browser/locale/browser.properties");
});

this.ContextualIdentityService = {
  _identities: [
    { userContextId: 1,
      icon: "chrome://browser/skin/usercontext/personal.svg",
      color: "#00a7e0",
      label: "userContextPersonal.label",
      accessKey: "userContextPersonal.accesskey" },
    { userContextId: 2,
      icon: "chrome://browser/skin/usercontext/work.svg",
      color: "#f89c24",
      label: "userContextWork.label",
      accessKey: "userContextWork.accesskey" },
    { userContextId: 3,
      icon: "chome://browser/skin/usercontext/banking.svg",
      color: "#7dc14c",
      label: "userContextBanking.label",
      accessKey: "userContextBanking.accesskey" },
    { userContextId: 4,
      icon: "chrome://browser/skin/usercontext/shopping.svg",
      color: "#ee5195",
      label: "userContextShopping.label",
      accessKey: "userContextShopping.accesskey" },
  ],

  _cssRule: false,

  getIdentities() {
    return this._identities;
  },

  getIdentityFromId(userContextId) {
    return this._identities.find(info => info.userContextId == userContextId);
  },

  getUserContextLabel(userContextId) {
    let identity = this.getIdentityFromId(userContextId);
    return gBrowserBundle.GetStringFromName(identity.label);
  },

  needsCssRule() {
    return !this._cssRule;
  },

  storeCssRule(cssRule) {
    this._cssRule = cssRule;
  },

  cssRules() {
    let rules = [];

    if (this.needsCssRule()) {
      return rules;
    }

    /* The CSS Rules for the ContextualIdentity tabs are set up like this:
     * 1. :root { --usercontext-color-<id>: #color }
     * 2. the template, replacing 'id' with the userContextId.
     * 3. tabbrowser-tab[usercontextid="<id>"] { background-image: var(--usercontext-tab-<id>) }
     */
    for (let identity of this._identities) {
      rules.push(":root { --usercontext-color-" + identity.userContextId + ": " + identity.color + " }");

      rules.push(this._cssRule.replace(/id/g, identity.userContextId));
      rules.push(".tabbrowser-tab[usercontextid=\"" + identity.userContextId + "\"] { background-image: var(--usercontext-tab-" + identity.userContextId + ") }");
    }

    return rules;
  },
}
