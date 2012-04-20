/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

let doc;
let tempScope = {};
Cu.import("resource:///modules/devtools/CssRuleView.jsm", tempScope);
let inplaceEditor = tempScope._getInplaceEditorForSpan;

XPCOMUtils.defineLazyGetter(this, "osString", function() {
  return Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).OS;
});

function ruleViewFrame()
{
  return InspectorUI.sidebar._tools["ruleview"].frame;
}

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

function waitForEditorFocus(aParent, aCallback)
{
  aParent.addEventListener("focus", function onFocus(evt) {
    if (inplaceEditor(evt.target)) {
      aParent.removeEventListener("focus", onFocus, true);
      let editor = inplaceEditor(evt.target);
      executeSoon(function() {
        aCallback(editor);
      });
    }
  }, true);
}

function waitForEditorBlur(aEditor, aCallback)
{
  let input = aEditor.input;
  input.addEventListener("blur", function onBlur() {
    input.removeEventListener("blur", onBlur, false);
    executeSoon(function() {
      aCallback();
    });
  }, false);
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

  InspectorUI.currentInspector.once("sidebaractivated-ruleview", testClip)
  InspectorUI.sidebar.show();
  InspectorUI.sidebar.activatePanel("ruleview");
}

function testClip()
{
  executeSoon(function() {
    info("Checking that _onCopyRule() returns " +
         "the correct clipboard value");
    let expectedPattern = "element {[\\r\\n]+" +
      "    margin: 10em;[\\r\\n]+" +
      "    font-size: 14pt;[\\r\\n]+" +
      "    font-family: helvetica,sans-serif;[\\r\\n]+" +
      "    color: rgb\\(170, 170, 170\\);[\\r\\n]+" +
      "}[\\r\\n]*";

    SimpleTest.waitForClipboard(function IUI_boundCopyPropCheck() {
        return checkClipboardData(expectedPattern);
      },
      checkCopyRule, checkCopyRuleWithEditorSelected, function() {
        failedClipboard(expectedPattern, checkCopyRuleWithEditorSelected);
      });
  });
}

function checkCopyRule() {
  let contentDoc = ruleViewFrame().contentDocument;
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
    ruleViewFrame().contentWindow);

  ruleView()._boundCopyRule();
  let menu = contentDoc.querySelector("#rule-view-context-menu");
  ok(menu, "we have the context menu");
  menu.hidePopup();
}

function checkCopyRuleWithEditorSelected()
{
  let contentDoc = ruleViewFrame().contentDocument;
  let rows = contentDoc.querySelectorAll(".rule-view-row");
  let propNodes = contentDoc.querySelectorAll(".ruleview-property");
  let propNode = propNodes[2];
  let propNameNode = propNode.querySelector(".ruleview-propertyname");

  ok(propNameNode, "we have the property name node");

  info("Checking that _boundCopyRule()  returns the correct clipboard value");
  let expectedPattern = "element {[\\r\\n]+" +
    "    margin: 10em;[\\r\\n]+" +
    "    font-size: 14pt;[\\r\\n]+" +
    "    font-family: helvetica,sans-serif;[\\r\\n]+" +
    "    color: rgb\\(170, 170, 170\\);[\\r\\n]+" +
    "}[\\r\\n]*";

  let elementRuleEditor = rows[0]._ruleEditor;
  waitForEditorFocus(elementRuleEditor.element, function onNewElement(aEditor) {
    ok(aEditor, "we have the editor");

    waitForBlur.editor = aEditor;

    // We need the context menu to open in the correct place in order for
    // popupNode to be propertly set.
    EventUtils.synthesizeMouse(aEditor.input, 1, 1,
      { type: "contextmenu", button: 2 }, ruleViewFrame().contentWindow);

    SimpleTest.waitForClipboard(function IUI_boundCopyCheckWithSelection() {
      let menu = contentDoc.querySelector("#rule-view-context-menu");
      ok(menu, "we have the context menu");
      menu.hidePopup();

      return checkClipboardData(expectedPattern);
    }, ruleView()._boundCopyRule, waitForBlur, function() {
      failedClipboard(expectedPattern, checkCopyProperty);
    });
  });
  EventUtils.synthesizeMouse(propNameNode, 1, 1, { }, ruleViewFrame().contentWindow);
}

function waitForBlur()
{
  waitForEditorBlur(waitForBlur.editor, function() {
    waitForBlur.editor = null;
    checkCopyProperty();
  });
  waitForBlur.editor.input.blur();
}

function checkCopyProperty()
{
  let contentDoc = ruleViewFrame().contentDocument;
  let props = contentDoc.querySelectorAll(".ruleview-property");
  let prop = props[2];

  info("Checking that _onCopyDeclaration() returns " +
       "the correct clipboard value");
  let expectedPattern = "font-family: helvetica,sans-serif;";

  // We need the context menu to open in the correct place in order for
  // popupNode to be propertly set.
  EventUtils.synthesizeMouse(prop, 1, 1, { type: "contextmenu", button: 2 },
    ruleViewFrame().contentWindow);

  SimpleTest.waitForClipboard(function IUI_boundCopyPropCheck() {
    return checkClipboardData(expectedPattern);
  },
  ruleView()._boundCopyDeclaration,
  checkCopyPropertyName, function() {
    failedClipboard(expectedPattern, checkCopyPropertyName);
  });
}

function checkCopyPropertyName()
{
  info("Checking that _onCopyProperty() returns " +
       "the correct clipboard value");
  let expectedPattern = "font-family";

  SimpleTest.waitForClipboard(function IUI_boundCopyPropNameCheck() {
    return checkClipboardData(expectedPattern);
  },
  ruleView()._boundCopyProperty,
  checkCopyPropertyValue, function() {
    failedClipboard(expectedPattern, checkCopyPropertyValue);
  });
}

function checkCopyPropertyValue()
{
  info("Checking that _onCopyPropertyValue() " +
       " returns the correct clipboard value");
  let expectedPattern = "helvetica,sans-serif";

  SimpleTest.waitForClipboard(function IUI_boundCopyPropValueCheck() {
    return checkClipboardData(expectedPattern);
  },
  ruleView()._boundCopyPropertyValue,
  checkCopySelection, function() {
    failedClipboard(expectedPattern, checkCopySelection);
  });
}

function checkCopySelection()
{
  let contentDoc = ruleViewFrame().contentDocument;
  let props = contentDoc.querySelectorAll(".ruleview-property");

  let range = document.createRange();
  range.setStart(props[0], 0);
  range.setEnd(props[4], 8);

  let selection = ruleViewFrame().contentWindow.getSelection();
  selection.addRange(range);

  info("Checking that _boundCopy()  returns the correct" +
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
  },ruleView()._boundCopy, finishup, function() {
    failedClipboard(expectedPattern, finishup);
  });
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
  InspectorUI.sidebar.hide();
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
