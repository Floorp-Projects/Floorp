/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the style inspector works properly

let doc, computedView, inspector;

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onBrowserLoad(evt) {
    gBrowser.selectedBrowser.removeEventListener("load", onBrowserLoad, true);
    doc = content.document;
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html,computed view context menu test";
}

function createDocument()
{
  doc.body.innerHTML = '<style type="text/css"> ' +
    'span { font-variant: small-caps; color: #000000; } ' +
    '.nomatches {color: #ff0000;}</style> <div id="first" style="margin: 10em; ' +
    'font-size: 14pt; font-family: helvetica, sans-serif; color: #AAA">\n' +
    '<h1>Some header text</h1>\n' +
    '<p id="salutation" style="font-size: 12pt">hi.</p>\n' +
    '<p id="body" style="font-size: 12pt">I am a test-case. This text exists ' +
    'solely to provide some things to <span style="color: yellow">' +
    'highlight</span> and <span style="font-weight: bold">count</span> ' +
    'style list-items in the box at right. If you are reading this, ' +
    'you should go do something else instead. Maybe read a book. Or better ' +
    'yet, write some test-cases for another bit of code. ' +
    '<span style="font-style: italic">some text</span></p>\n' +
    '<p id="closing">more text</p>\n' +
    '<p>even more text</p>' +
    '</div>';
  doc.title = "Computed view keyboard navigation test";

  openComputedView(startTests);
}

function startTests(aInspector, aComputedView)
{
  computedView = aComputedView;
  inspector = aInspector;
  testTabThrougStyles();
}

function endTests()
{
  computedView = inspector = doc = null;
  gBrowser.removeCurrentTab();
  finish();
}

function testTabThrougStyles()
{
  let span = doc.querySelector("span");

  inspector.once("computed-view-refreshed", () => {
    // Selecting the first computed style in the list
    let firstStyle = computedView.styleDocument.querySelector(".property-view");
    ok(firstStyle, "First computed style found in panel");
    firstStyle.focus();

    // Tab to select the 2nd style, press return
    EventUtils.synthesizeKey("VK_TAB", {});
    EventUtils.synthesizeKey("VK_RETURN", {});
    inspector.once("computed-view-property-expanded", () => {
      // Verify the 2nd style has been expanded
      let secondStyleSelectors = computedView.styleDocument.querySelectorAll(
        ".property-content .matchedselectors")[1];
      ok(secondStyleSelectors.childNodes.length > 0, "Matched selectors expanded");

      // Tab back up and test the same thing, with space
      EventUtils.synthesizeKey("VK_TAB", {shiftKey: true});
      EventUtils.synthesizeKey("VK_SPACE", {});
      inspector.once("computed-view-property-expanded", () => {
        // Verify the 1st style has been expanded too
        let firstStyleSelectors = computedView.styleDocument.querySelectorAll(
          ".property-content .matchedselectors")[0];
        ok(firstStyleSelectors.childNodes.length > 0, "Matched selectors expanded");

        endTests();
      });
    });
  });

  inspector.selection.setNode(span);
}
