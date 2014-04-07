/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check stylesheets on HMTL and XUL document

const TEST_URI_HTML = TEST_URL_ROOT + "doc_content_stylesheet.html";
const TEST_URI_XUL = TEST_URL_ROOT + "doc_content_stylesheet.xul";
const XUL_URI = Cc["@mozilla.org/network/io-service;1"]
                .getService(Ci.nsIIOService)
                .newURI(TEST_URI_XUL, null, null);
const XUL_PRINCIPAL = Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                        .getService(Ci.nsIScriptSecurityManager)
                        .getNoAppCodebasePrincipal(XUL_URI);

let {CssLogic} = devtools.require("devtools/styleinspector/css-logic");

let test = asyncTest(function*() {
  info("Checking stylesheets on HTML document");
  yield addTab(TEST_URI_HTML);
  let target = getNode("#target");

  let {toolbox, inspector, view} = yield openRuleView();
  yield selectNode(target, inspector);

  info("Checking stylesheets");
  checkSheets(target);

  info("Checking stylesheets on XUL document");
  info("Allowing XUL content");
  allowXUL();
  yield addTab(TEST_URI_XUL);

  let {toolbox, inspector, view} = yield openRuleView();
  let target = getNode("#target");
  yield selectNode(target, inspector);

  checkSheets(target);
  info("Disallowing XUL content");
  disallowXUL();
});

function allowXUL() {
  Cc["@mozilla.org/permissionmanager;1"].getService(Ci.nsIPermissionManager)
    .addFromPrincipal(XUL_PRINCIPAL, 'allowXULXBL', Ci.nsIPermissionManager.ALLOW_ACTION);
}

function disallowXUL() {
  Cc["@mozilla.org/permissionmanager;1"].getService(Ci.nsIPermissionManager)
    .addFromPrincipal(XUL_PRINCIPAL, 'allowXULXBL', Ci.nsIPermissionManager.DENY_ACTION);
}

function checkSheets(target) {
  let domUtils = Cc["@mozilla.org/inspector/dom-utils;1"]
      .getService(Ci.inIDOMUtils);
  let domRules = domUtils.getCSSStyleRules(target);

  for (let i = 0, n = domRules.Count(); i < n; i++) {
    let domRule = domRules.GetElementAt(i);
    let sheet = domRule.parentStyleSheet;
    let isContentSheet = CssLogic.isContentStylesheet(sheet);

    if (!sheet.href ||
        /doc_content_stylesheet_/.test(sheet.href)) {
      ok(isContentSheet, sheet.href + " identified as content stylesheet");
    } else {
      ok(!isContentSheet, sheet.href + " identified as non-content stylesheet");
    }
  }
}
