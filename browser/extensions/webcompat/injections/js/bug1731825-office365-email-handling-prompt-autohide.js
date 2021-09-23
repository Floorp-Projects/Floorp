"use strict";

/**
 * Bug 1731825 - Office 365 email handling prompt autohide
 *
 * This site patch prevents the notification bar on Office 365
 * apps from popping up on each page-load, offering to handle
 * email with Outlook.
 */

/* globals exportFunction */

const warning =
  "Office 365 Outlook email handling prompt has been hidden. See https://bugzilla.mozilla.org/show_bug.cgi?id=1731825 for details.";

const localStorageKey = "mailProtocolHandlerAlreadyOffered";

const nav = navigator.wrappedJSObject;
const { registerProtocolHandler } = nav;
const { localStorage } = window.wrappedJSObject;

Object.defineProperty(navigator.wrappedJSObject, "registerProtocolHandler", {
  value: exportFunction(function(scheme, url, title) {
    if (localStorage.getItem(localStorageKey)) {
      console.info(warning);
      return undefined;
    }
    registerProtocolHandler.call(nav, scheme, url, title);
    localStorage.setItem(localStorageKey, true);
    return undefined;
  }, window),
});
