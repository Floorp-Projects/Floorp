/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This content script should work in any browser or iframe and should not
 * depend on the frame being contained in tabbrowser. */

/* eslint-env mozilla/frame-script */
/* eslint no-unused-vars: ["error", {args: "none"}] */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

// TabChildGlobal
var global = this;

XPCOMUtils.defineLazyModuleGetters(this, {
  ContentLinkHandler: "resource:///modules/ContentLinkHandler.jsm",
  ContentMetaHandler: "resource:///modules/ContentMetaHandler.jsm",
  LoginFormFactory: "resource://gre/modules/LoginManagerContent.jsm",
  InsecurePasswordUtils: "resource://gre/modules/InsecurePasswordUtils.jsm",
  FormSubmitObserver: "resource:///modules/FormSubmitObserver.jsm",
  ContextMenuChild: "resource:///actors/ContextMenuChild.jsm",
});

XPCOMUtils.defineLazyGetter(this, "LoginManagerContent", () => {
  let tmp = {};
  ChromeUtils.import("resource://gre/modules/LoginManagerContent.jsm", tmp);
  tmp.LoginManagerContent.setupEventListeners(global);
  return tmp.LoginManagerContent;
});

XPCOMUtils.defineLazyProxy(this, "formSubmitObserver", () => {
  return new FormSubmitObserver(content, this);
}, {
  // stub QI
  QueryInterface: ChromeUtils.generateQI([Ci.nsIFormSubmitObserver, Ci.nsISupportsWeakReference])
});

Services.obs.addObserver(formSubmitObserver, "invalidformsubmit", true);

// NOTE: Much of this logic is duplicated in BrowserCLH.js for Android.
addMessageListener("RemoteLogins:fillForm", function(message) {
  // intercept if ContextMenu.jsm had sent a plain object for remote targets
  message.objects.inputElement = ContextMenuChild.getTarget(global, message, "inputElement");
  LoginManagerContent.receiveMessage(message, content);
});
addEventListener("DOMFormHasPassword", function(event) {
  LoginManagerContent.onDOMFormHasPassword(event, content);
  let formLike = LoginFormFactory.createFromForm(event.originalTarget);
  InsecurePasswordUtils.reportInsecurePasswords(formLike);
});
addEventListener("DOMInputPasswordAdded", function(event) {
  LoginManagerContent.onDOMInputPasswordAdded(event, content);
  let formLike = LoginFormFactory.createFromField(event.originalTarget);
  InsecurePasswordUtils.reportInsecurePasswords(formLike);
});
addEventListener("DOMAutoComplete", function(event) {
  LoginManagerContent.onUsernameInput(event);
});

new ContentLinkHandler(this);
ContentMetaHandler.init(this);

// This is a temporary hack to prevent regressions (bug 1471327).
void content;

addEventListener("DOMWindowFocus", function(event) {
  sendAsyncMessage("DOMWindowFocus", {});
}, false);
