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

let inspector, ruleView;
let {CssLogic} = devtools.require("devtools/styleinspector/css-logic");

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
    openRuleView((aInspector, aRuleView) => {
      inspector = aInspector;
      ruleView = aRuleView;
      inspector.selection.setNode(target);
      inspector.once("inspector-updated", testModifyRules);
    });
  });
}

function reselectElement(target, cb)
{
  inspector.selection.setNode(target.parentNode);
  inspector.once("inspector-updated", ()=> {
    inspector.selection.setNode(target);
    inspector.once("inspector-updated", cb);
  });
}

function testModifyRules()
{
  // Set a property on all rules, then refresh and make sure they are still
  // there (and there wasn't an error on the server side)
  for (let rule of ruleView._elementStyle.rules) {
    rule.editor.addProperty("font-weight", "bold", "");
  }

  executeSoon(() => {
    reselectElement(doc.querySelector("#target"), () => {
      for (let rule of ruleView._elementStyle.rules) {
        let lastRule = rule.textProps[rule.textProps.length - 1];

        is (lastRule.name, "font-weight", "Last rule name is font-weight");
        is (lastRule.value, "bold", "Last rule value is bold");
      }

      gBrowser.removeCurrentTab();
      openXUL();
    });
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
  doc = inspector = ruleView = null;
  gBrowser.removeCurrentTab();
  finish();
}
