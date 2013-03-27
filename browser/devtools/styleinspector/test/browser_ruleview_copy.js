/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

let doc;
let inspector;
let win;

XPCOMUtils.defineLazyGetter(this, "osString", function() {
  return Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).OS;
});

function createDocument()
{
  doc.body.innerHTML = '<style type="text/css"> ' +
    'html { color: #000000; } ' +
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
  doc.title = "Rule view context menu test";

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
    inspector = toolbox.getCurrentPanel();
    inspector.sidebar.select("ruleview");
    win = inspector.sidebar.getWindowForTab("ruleview");
    highlightNode();
  });
}

function highlightNode()
{
  // Highlight a node.
  let div = content.document.getElementsByTagName("div")[0];

  inspector.selection.once("new-node", function() {
    is(inspector.selection.node, div, "selection matches the div element");
    executeSoon(checkCopySelection);
  });
  executeSoon(function() {
    inspector.selection.setNode(div);
  });
}

function checkCopySelection()
{
  let contentDoc = win.document;
  let props = contentDoc.querySelectorAll(".ruleview-property");
  let values = contentDoc.querySelectorAll(".ruleview-propertycontainer");

  let range = document.createRange();
  range.setStart(props[0], 0);
  range.setEnd(values[4], 2);

  let selection = win.getSelection();
  selection.addRange(range);

  info("Checking that _boundCopy() returns the correct " +
    "clipboard value");
  let expectedPattern = "    margin: 10em;[\\r\\n]+" +
                        "    font-size: 14pt;[\\r\\n]+" +
                        "    font-family: helvetica,sans-serif;[\\r\\n]+" +
                        "    color: rgb\\(170, 170, 170\\);[\\r\\n]+" +
                        "}[\\r\\n]+" +
                        "html {[\\r\\n]+" +
                        "    color: rgb\\(0, 0, 0\\);[\\r\\n]*";

  SimpleTest.waitForClipboard(function IUI_boundCopyCheck() {
    return checkClipboardData(expectedPattern);
  },function() {fireCopyEvent(props[0])}, finishup, function() {
    failedClipboard(expectedPattern, finishup);
  });
}

function selectNode(aNode) {
  let doc = aNode.ownerDocument;
  let win = doc.defaultView;
  let range = doc.createRange();

  range.selectNode(aNode);
  win.getSelection().addRange(range);
}

function checkClipboardData(aExpectedPattern)
{
  let actual = SpecialPowers.getClipboardData("text/unicode");
  let expectedRegExp = new RegExp(aExpectedPattern, "g");
  return expectedRegExp.test(actual);
}

function failedClipboard(aExpectedPattern, aCallback)
{
  // Format expected text for comparison
  let terminator = osString == "WINNT" ? "\r\n" : "\n";
  aExpectedPattern = aExpectedPattern.replace(/\[\\r\\n\][+*]/g, terminator);
  aExpectedPattern = aExpectedPattern.replace(/\\\(/g, "(");
  aExpectedPattern = aExpectedPattern.replace(/\\\)/g, ")");

  let actual = SpecialPowers.getClipboardData("text/unicode");

  // Trim the right hand side of our strings. This is because expectedPattern
  // accounts for windows sometimes adding a newline to our copied data.
  aExpectedPattern = aExpectedPattern.trimRight();
  actual = actual.trimRight();

  dump("TEST-UNEXPECTED-FAIL | Clipboard text does not match expected ... " +
    "results (escaped for accurate comparison):\n");
  info("Actual: " + escape(actual));
  info("Expected: " + escape(aExpectedPattern));
  aCallback();
}

function finishup()
{
  gBrowser.removeCurrentTab();
  doc = inspector = null;
  finish();
}

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, arguments.callee,
      true);
    doc = content.document;
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html,<p>rule view context menu test</p>";
}
