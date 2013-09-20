/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

let doc;
let inspector;
let view;
let viewDoc;

const DOCUMENT_URL = "data:text/html," + encodeURIComponent([
  '<html>' +
  '<head>' +
  '  <title>Computed view toggling test</title>',
  '  <style type="text/css"> ',
  '    html { color: #000000; font-size: 15pt; } ',
  '    h1 { color: red; } ',
  '  </style>',
  '</head>',
  '<body>',
  '  <h1>Some header text</h1>',
  '</body>',
  '</html>'
].join("\n"));

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, arguments.callee,
      true);
    doc = content.document;
    waitForFocus(function () { openComputedView(startTests); }, content);
  }, true);

  content.location = DOCUMENT_URL;
}

function startTests(aInspector, aview)
{
  inspector = aInspector;
  view = aview;
  viewDoc = view.styleDocument;

  testExpandOnTwistyClick();
}

function endTests()
{
  doc = inspector = view = viewDoc = null;
  gBrowser.removeCurrentTab();
  finish();
}

function testExpandOnTwistyClick()
{
  let h1 = doc.querySelector("h1");
  ok(h1, "H1 exists");

  inspector.selection.setNode(h1);
  inspector.once("inspector-updated", () => {
    // Get the first twisty
    let twisty = viewDoc.querySelector(".expandable");
    ok(twisty, "Twisty found");

    // Click and check whether it's been expanded
    inspector.once("computed-view-property-expanded", () => {
      // Expanded means the matchedselectors div is not empty
      let div = viewDoc.querySelector(".property-content .matchedselectors");
      ok(div.childNodes.length > 0, "Matched selectors are expanded on twisty click");

      testCollapseOnTwistyClick();
    });
    twisty.click();
  });
}

function testCollapseOnTwistyClick() {
  // Get the same first twisty again
  let twisty = viewDoc.querySelector(".expandable");
  ok(twisty, "Twisty found");

  // Click and check whether matched selectors are collapsed now
  inspector.once("computed-view-property-collapsed", () => {
    // Collapsed means the matchedselectors div is empty
    let div = viewDoc.querySelector(".property-content .matchedselectors");
    ok(div.childNodes.length === 0, "Matched selectors are collapsed on twisty click");

    testExpandOnDblClick();
  });
  twisty.click();
}

function testExpandOnDblClick()
{
  // Get the computed rule container, not the twisty this time
  let container = viewDoc.querySelector(".property-view");

  // Dblclick on it and check if it expands the matched selectors
  inspector.once("computed-view-property-expanded", () => {
    // Expanded means the matchedselectors div is not empty
    let div = viewDoc.querySelector(".property-content .matchedselectors");
    ok(div.childNodes.length > 0, "Matched selectors are expanded on dblclick");

    testCollapseOnDblClick();
  });
  EventUtils.synthesizeMouseAtCenter(container, {clickCount: 2}, view.styleWindow);
}

function testCollapseOnDblClick()
{
  // Get the computed rule container, not the twisty this time
  let container = viewDoc.querySelector(".property-view");

  // Dblclick on it and check if it expands the matched selectors
  inspector.once("computed-view-property-collapsed", () => {
    // Collapsed means the matchedselectors div is empty
    let div = viewDoc.querySelector(".property-content .matchedselectors");
    ok(div.childNodes.length === 0, "Matched selectors are collapsed on dblclick");

    endTests();
  });
  EventUtils.synthesizeMouseAtCenter(container, {clickCount: 2}, view.styleWindow);
}
