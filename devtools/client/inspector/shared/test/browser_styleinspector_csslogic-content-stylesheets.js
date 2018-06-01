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
const XUL_URI = Services.io.newURI(TEST_URI_XUL);
const XUL_PRINCIPAL = Services.scriptSecurityManager.createCodebasePrincipal(XUL_URI, {});

add_task(async function() {
  requestLongerTimeout(2);

  info("Checking stylesheets on HTML document");
  await addTab(TEST_URI_HTML);

  let {inspector, testActor} = await openInspector();
  await selectNode("#target", inspector);

  info("Checking stylesheets");
  await checkSheets("#target", testActor);

  info("Checking authored stylesheets");
  await addTab(TEST_URI_AUTHOR);

  ({inspector} = await openInspector());
  await selectNode("#target", inspector);
  await checkSheets("#target", testActor);

  info("Checking stylesheets on XUL document");
  info("Allowing XUL content");
  allowXUL();
  await addTab(TEST_URI_XUL);

  ({inspector} = await openInspector());
  await selectNode("#target", inspector);

  await checkSheets("#target", testActor);
  info("Disallowing XUL content");
  disallowXUL();
});

function allowXUL() {
  Services.perms.addFromPrincipal(XUL_PRINCIPAL, "allowXULXBL",
    Ci.nsIPermissionManager.ALLOW_ACTION);
}

function disallowXUL() {
  Services.perms.addFromPrincipal(XUL_PRINCIPAL, "allowXULXBL",
    Ci.nsIPermissionManager.DENY_ACTION);
}

async function checkSheets(targetSelector, testActor) {
  const sheets = await testActor.getStyleSheetsInfoForNode(targetSelector);

  for (const sheet of sheets) {
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
