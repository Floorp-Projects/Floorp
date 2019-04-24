/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This content script should work in any browser or iframe and should not
 * depend on the frame being contained in tabbrowser. */

/* eslint-env mozilla/frame-script */
/* eslint no-unused-vars: ["error", {args: "none"}] */

var {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

// BrowserChildGlobal
var global = this;

XPCOMUtils.defineLazyModuleGetters(this, {
  ContentMetaHandler: "resource:///modules/ContentMetaHandler.jsm",
  LoginFormFactory: "resource://gre/modules/LoginFormFactory.jsm",
  InsecurePasswordUtils: "resource://gre/modules/InsecurePasswordUtils.jsm",
  ContextMenuChild: "resource:///actors/ContextMenuChild.jsm",
});

XPCOMUtils.defineLazyGetter(this, "LoginManagerContent", () => {
  let tmp = {};
  ChromeUtils.import("resource://gre/modules/LoginManagerContent.jsm", tmp);
  tmp.LoginManagerContent.setupEventListeners(global);
  return tmp.LoginManagerContent;
});

// NOTE: Much of this logic is duplicated in BrowserCLH.js for Android.
addMessageListener("PasswordManager:fillForm", function(message) {
  // intercept if ContextMenu.jsm had sent a plain object for remote targets
  message.objects.inputElement = ContextMenuChild.getTarget(global, message, "inputElement");
  LoginManagerContent.receiveMessage(message, content);
});

function shouldIgnoreLoginManagerEvent(event) {
  // If we have a null principal then prevent any more password manager code from running and
  // incorrectly using the document `location`.
  return event.target.nodePrincipal.isNullPrincipal;
}

addEventListener("DOMFormBeforeSubmit", function(event) {
  if (shouldIgnoreLoginManagerEvent(event)) {
    return;
  }
  LoginManagerContent.onDOMFormBeforeSubmit(event);
});
addEventListener("DOMFormHasPassword", function(event) {
  if (shouldIgnoreLoginManagerEvent(event)) {
    return;
  }
  LoginManagerContent.onDOMFormHasPassword(event);
  let formLike = LoginFormFactory.createFromForm(event.originalTarget);
  InsecurePasswordUtils.reportInsecurePasswords(formLike);
});
addEventListener("DOMInputPasswordAdded", function(event) {
  if (shouldIgnoreLoginManagerEvent(event)) {
    return;
  }
  LoginManagerContent.onDOMInputPasswordAdded(event, content);
  let formLike = LoginFormFactory.createFromField(event.originalTarget);
  InsecurePasswordUtils.reportInsecurePasswords(formLike);
});
addEventListener("DOMAutoComplete", function(event) {
  if (shouldIgnoreLoginManagerEvent(event)) {
    return;
  }
  LoginManagerContent.onDOMAutoComplete(event);
});

ContentMetaHandler.init(this);

// This is a temporary hack to prevent regressions (bug 1471327).
void content;

addEventListener("DOMWindowFocus", function(event) {
  sendAsyncMessage("DOMWindowFocus", {});
}, false);
