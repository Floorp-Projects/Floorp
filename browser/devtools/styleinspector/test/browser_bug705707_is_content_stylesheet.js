/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the correct stylesheets origins are identified in HTML & XUL
// stylesheets

let doc;

const TEST_URI = "http://example.com/browser/browser/devtools/styleinspector/" +
                 "test/browser_bug705707_is_content_stylesheet.html";
const TEST_URI2 = "http://example.com/browser/browser/devtools/styleinspector/" +
                 "test/browser_bug705707_is_content_stylesheet.xul";
const XUL_URI = Cc["@mozilla.org/network/io-service;1"]
                .getService(Ci.nsIIOService)
                .newURI(TEST_URI2, null, null);
const XUL_PRINCIPAL =  Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                                 .getService(Ci.nsIScriptSecurityManager)
                                 .getNoAppCodebasePrincipal(XUL_URI);

let tempScope = {};
Cu.import("resource:///modules/devtools/CssLogic.jsm", tempScope);
let CssLogic = tempScope.CssLogic;

function test()
{
  waitForExplicitFinish();
  addTab(TEST_URI);
  browser.addEventListener("load", htmlLoaded, true);
}

function htmlLoaded()
{
  browser.removeEventListener("load", htmlLoaded, true);
  doc = content.document;
  testFromHTML()
}

function testFromHTML()
{
  let target = doc.querySelector("#target");

  executeSoon(function() {
    checkSheets(target);
    gBrowser.removeCurrentTab();
    openXUL();
  });
}

function openXUL()
{
  Cc["@mozilla.org/permissionmanager;1"].getService(Ci.nsIPermissionManager)
    .addFromPrincipal(XUL_PRINCIPAL, 'allowXULXBL', Ci.nsIPermissionManager.ALLOW_ACTION);
  addTab(TEST_URI2);
  browser.addEventListener("load", xulLoaded, true);
}

function xulLoaded()
{
  browser.removeEventListener("load", xulLoaded, true);
  doc = content.document;
  testFromXUL()
}

function testFromXUL()
{
  let target = doc.querySelector("#target");

  executeSoon(function() {
    checkSheets(target);
    finishUp();
  });
}

function checkSheets(aTarget)
{
  let domUtils = Cc["@mozilla.org/inspector/dom-utils;1"]
      .getService(Ci.inIDOMUtils);
  let domRules = domUtils.getCSSStyleRules(aTarget);

  for (let i = 0, n = domRules.Count(); i < n; i++) {
    let domRule = domRules.GetElementAt(i);
    let sheet = domRule.parentStyleSheet;
    let isContentSheet = CssLogic.isContentStylesheet(sheet);

    if (!sheet.href ||
        /browser_bug705707_is_content_stylesheet_/.test(sheet.href)) {
      ok(isContentSheet, sheet.href + " identified as content stylesheet");
    } else {
      ok(!isContentSheet, sheet.href + " identified as non-content stylesheet");
    }
  }
}

function finishUp()
{
  info("finishing up");
  Cc["@mozilla.org/permissionmanager;1"].getService(Ci.nsIPermissionManager)
    .addFromPrincipal(XUL_PRINCIPAL, 'allowXULXBL', Ci.nsIPermissionManager.DENY_ACTION);
  doc = null;
  gBrowser.removeCurrentTab();
  finish();
}
