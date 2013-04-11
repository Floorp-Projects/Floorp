/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that we correctly display appropriate media query titles in the
// rule view.

let doc;

const TEST_URI = "http://example.com/browser/browser/devtools/styleinspector/" +
  "test/browser_bug722196_identify_media_queries.html";

function test()
{
  waitForExplicitFinish();
  addTab(TEST_URI);
  browser.addEventListener("load", docLoaded, true);
}

function docLoaded()
{
  browser.removeEventListener("load", docLoaded, true);
  doc = content.document;
  checkSheets();
}

function checkSheets()
{
  var div = doc.querySelector("div");
  ok(div, "captain, we have the div");

  let elementStyle = new _ElementStyle(div);
  is(elementStyle.rules.length, 3, "Should have 3 rules.");

  let _strings = Services.strings
    .createBundle("chrome://browser/locale/devtools/styleinspector.properties");

  let inline = _strings.GetStringFromName("rule.sourceInline");

  is(elementStyle.rules[0].title, inline, "check rule 0 title");
  is(elementStyle.rules[1].title, inline +
    ":15 @media screen and (min-width: 1px)", "check rule 1 title");
  is(elementStyle.rules[2].title, inline + ":8", "check rule 2 title");
  finishUp();
}

function finishUp()
{
  doc = null;
  gBrowser.removeCurrentTab();
  finish();
}
