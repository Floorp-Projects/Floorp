/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check stylesheets on HMTL and XUL document

// FIXME: this test opens the devtools for nothing, it should be changed into a
// devtools/server/tests/mochitest/test_css-logic-...something...html
// test

const TEST_URI_HTML = TEST_URL_ROOT + "doc_content_stylesheet.html";
const TEST_URI_AUTHOR = TEST_URL_ROOT + "doc_author-sheet.html";
const TEST_URI_XUL = TEST_URL_ROOT + "doc_content_stylesheet.xul";
const XUL_URI = Cc["@mozilla.org/network/io-service;1"]
                .getService(Ci.nsIIOService)
                .newURI(TEST_URI_XUL);
var ssm = Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                            .getService(Ci.nsIScriptSecurityManager);
const XUL_PRINCIPAL = ssm.createCodebasePrincipal(XUL_URI, {});

add_task(function* () {
  requestLongerTimeout(2);

  info("Checking stylesheets on HTML document");
  yield addTab(TEST_URI_HTML);

  let {inspector, testActor} = yield openInspector();
  yield selectNode("#target", inspector);

  info("Checking stylesheets");
  yield checkSheets("#target", testActor);

  info("Checking authored stylesheets");
  yield addTab(TEST_URI_AUTHOR);

  ({inspector} = yield openInspector());
  yield selectNode("#target", inspector);
  yield checkSheets("#target", testActor);

  info("Checking stylesheets on XUL document");
  info("Allowing XUL content");
  allowXUL();
  yield addTab(TEST_URI_XUL);

  ({inspector} = yield openInspector());
  yield selectNode("#target", inspector);

  yield checkSheets("#target", testActor);
  info("Disallowing XUL content");
  disallowXUL();
});

function allowXUL() {
  Cc["@mozilla.org/permissionmanager;1"].getService(Ci.nsIPermissionManager)
    .addFromPrincipal(XUL_PRINCIPAL, "allowXULXBL",
      Ci.nsIPermissionManager.ALLOW_ACTION);
}

function disallowXUL() {
  Cc["@mozilla.org/permissionmanager;1"].getService(Ci.nsIPermissionManager)
    .addFromPrincipal(XUL_PRINCIPAL, "allowXULXBL",
      Ci.nsIPermissionManager.DENY_ACTION);
}

function* checkSheets(targetSelector, testActor) {
  let sheets = yield testActor.getStyleSheetsInfoForNode(targetSelector);

  for (let sheet of sheets) {
    if (!sheet.href ||
        /doc_content_stylesheet_/.test(sheet.href) ||
        // For the "authored" case.
        /^data:.*seagreen/.test(sheet.href)) {
      ok(sheet.isContentSheet,
        sheet.href + " identified as content stylesheet");
    } else {
      ok(!sheet.isContentSheet,
        sheet.href + " identified as non-content stylesheet");
    }
  }
}
