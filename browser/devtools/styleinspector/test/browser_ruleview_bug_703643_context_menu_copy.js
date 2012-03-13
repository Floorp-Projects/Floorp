/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

let doc;

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

  openInspector();
}

function openInspector()
{
  ok(window.InspectorUI, "InspectorUI variable exists");
  ok(!InspectorUI.inspecting, "Inspector is not highlighting");
  ok(InspectorUI.store.isEmpty(), "Inspector.store is empty");

  Services.obs.addObserver(inspectorUIOpen,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
  InspectorUI.openInspectorUI();
}

function inspectorUIOpen()
{
  Services.obs.removeObserver(inspectorUIOpen,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);

  // Make sure the inspector is open.
  ok(InspectorUI.inspecting, "Inspector is highlighting");
  ok(!InspectorUI.treePanel.isOpen(), "Inspector Tree Panel is not open");
  ok(!InspectorUI.isSidebarOpen, "Inspector Sidebar is not open");
  ok(!InspectorUI.store.isEmpty(), "InspectorUI.store is not empty");
  is(InspectorUI.store.length, 1, "Inspector.store.length = 1");

  // Highlight a node.
  let div = content.document.getElementsByTagName("div")[0];
  InspectorUI.inspectNode(div);
  InspectorUI.stopInspecting();
  is(InspectorUI.selection, div, "selection matches the div element");

  Services.obs.addObserver(testClip,
    InspectorUI.INSPECTOR_NOTIFICATIONS.RULEVIEWREADY, false);

  InspectorUI.showSidebar();
  InspectorUI.openRuleView();
}

function testClip()
{
  Services.obs.removeObserver(testClip,
    InspectorUI.INSPECTOR_NOTIFICATIONS.RULEVIEWREADY, false);

  executeSoon(function() {
    info("Checking that InspectorUI.ruleViewCopyRule() returns " +
         "the correct clipboard value");
    let expectedPattern = "element {[\\r\\n]+" +
      "    margin: 10em;[\\r\\n]+" +
      "    font-size: 14pt;[\\r\\n]+" +
      "    font-family: helvetica,sans-serif;[\\r\\n]+" +
      "    color: rgb\\(170, 170, 170\\);[\\r\\n]+" +
      "}[\\r\\n]*";
    info("Expected pattern: " + expectedPattern);

    SimpleTest.waitForClipboard(function IUI_boundCopyPropCheck() {
        return checkClipboardData(expectedPattern);
      },
      checkCopyRule, checkCopyProperty, checkCopyProperty);
  });
}

function checkCopyRule() {
  let ruleView = document.querySelector("#devtools-sidebar-iframe-ruleview");
  let contentDoc = ruleView.contentDocument;
  let props = contentDoc.querySelectorAll(".ruleview-property");

  is(props.length, 5, "checking property length");

  let prop = props[2];
  let propName = prop.querySelector(".ruleview-propertyname").textContent;
  let propValue = prop.querySelector(".ruleview-propertyvalue").textContent;

  is(propName, "font-family", "checking property name");
  is(propValue, "helvetica,sans-serif", "checking property value");

  // We need the context menu to open in the correct place in order for
  // popupNode to be propertly set.
  EventUtils.synthesizeMouse(prop, 1, 1, { type: "contextmenu", button: 2 },
    ruleView.contentWindow);

  InspectorUI.ruleViewCopyRule();
}

function checkCopyProperty()
{
  info("Checking that InspectorUI.cssRuleViewBoundCopyDeclaration() returns " +
       "the correct clipboard value");
  let expectedPattern = "font-family: helvetica,sans-serif;";
  info("Expected pattern: " + expectedPattern);

  SimpleTest.waitForClipboard(function IUI_boundCopyPropCheck() {
      return checkClipboardData(expectedPattern);
    },
    InspectorUI.cssRuleViewBoundCopyDeclaration,
    checkCopyPropertyName, checkCopyPropertyName);
}

function checkCopyPropertyName()
{
  info("Checking that InspectorUI.cssRuleViewBoundCopyProperty() returns " +
       "the correct clipboard value");
  let expectedPattern = "font-family";
  info("Expected pattern: " + expectedPattern);

  SimpleTest.waitForClipboard(function IUI_boundCopyPropNameCheck() {
      return checkClipboardData(expectedPattern);
    },
    InspectorUI.cssRuleViewBoundCopyProperty,
    checkCopyPropertyValue, checkCopyPropertyValue);
}

function checkCopyPropertyValue()
{
  info("Checking that InspectorUI.cssRuleViewBoundCopyPropertyValue() " +
       " returns the correct clipboard value");
  let expectedPattern = "helvetica,sans-serif";
  info("Expected pattern: " + expectedPattern);

  SimpleTest.waitForClipboard(function IUI_boundCopyPropValueCheck() {
      return checkClipboardData(expectedPattern);
    },
    InspectorUI.cssRuleViewBoundCopyPropertyValue,
    checkCopySelection, checkCopySelection);
}

function checkCopySelection()
{
  let ruleView = document.querySelector("#devtools-sidebar-iframe-ruleview");
  let contentDoc = ruleView.contentDocument;
  let props = contentDoc.querySelectorAll(".ruleview-property");

  let range = document.createRange();
  range.setStart(props[0], 0);
  range.setEnd(props[4], 8);
  ruleView.contentWindow.getSelection().addRange(range);

  info("Checking that InspectorUI.cssRuleViewBoundCopy()  returns the correct" +
       "clipboard value");
  let expectedPattern = "    margin: 10em;[\\r\\n]+" +
                        "    font-size: 14pt;[\\r\\n]+" +
                        "    font-family: helvetica,sans-serif;[\\r\\n]+" +
                        "    color: rgb\\(170, 170, 170\\);[\\r\\n]+" +
                        "}[\\r\\n]+" +
                        "html {[\\r\\n]+" +
                        "    color: rgb\\(0, 0, 0\\);[\\r\\n]*";
  info("Expected pattern: " + expectedPattern);

  SimpleTest.waitForClipboard(function IUI_boundCopyCheck() {
      return checkClipboardData(expectedPattern);
    },InspectorUI.cssRuleViewBoundCopy, finishup, finishup);
}

function checkClipboardData(aExpectedPattern)
{
  let actual = SpecialPowers.getClipboardData("text/unicode");
  let expectedRegExp = new RegExp(aExpectedPattern, "g");
  return expectedRegExp.test(actual);
}

function finishup()
{
  InspectorUI.closeInspectorUI();
  gBrowser.removeCurrentTab();
  doc = null;
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
