/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let tempScope = {}
Cu.import("resource:///modules/devtools/CssRuleView.jsm", tempScope);
let CssRuleView = tempScope.CssRuleView;
let _ElementStyle = tempScope._ElementStyle;
let _editableField = tempScope._editableField;
let inplaceEditor = tempScope._getInplaceEditorForSpan;

let doc;
let ruleDialog;
let ruleView;
let testElement;

function startTest()
{
  let style = '' +
    '#testid {' +
    '  background-color: blue;' +
    '} ' +
    '.testclass {' +
    '  background-color: green;' +
    '}';

  let styleNode = addStyle(doc, style);
  doc.body.innerHTML = '<div id="testid" class="testclass">Styled Node</div>';
  testElement = doc.getElementById("testid");

  let elementStyle = 'margin-top: 1px; padding-top: 5px;'
  testElement.setAttribute("style", elementStyle);

  ruleDialog = openDialog("chrome://browser/content/devtools/cssruleview.xul",
                          "cssruleviewtest",
                          "width=200,height=350");
  ruleDialog.addEventListener("load", function onLoad(evt) {
    ruleDialog.removeEventListener("load", onLoad);
    let doc = ruleDialog.document;
    ruleView = new CssRuleView(doc);
    doc.documentElement.appendChild(ruleView.element);
    ruleView.highlight(testElement);
    waitForFocus(testRuleChanges, ruleDialog);
  }, true);
}

function testRuleChanges()
{
  let selectors = ruleView.doc.querySelectorAll(".ruleview-selector");
  is(selectors.length, 3, "Three rules visible.");
  is(selectors[0].textContent.indexOf("element"), 0, "First item is inline style.");
  is(selectors[1].textContent.indexOf("#testid"), 0, "Second item is id rule.");
  is(selectors[2].textContent.indexOf(".testclass"), 0, "Third item is class rule.");

  // Change the id and refresh.
  testElement.setAttribute("id", "differentid");
  ruleView.nodeChanged();

  let selectors = ruleView.doc.querySelectorAll(".ruleview-selector");
  is(selectors.length, 2, "Three rules visible.");
  is(selectors[0].textContent.indexOf("element"), 0, "First item is inline style.");
  is(selectors[1].textContent.indexOf(".testclass"), 0, "Second item is class rule.");

  testElement.setAttribute("id", "testid");
  ruleView.nodeChanged();

  // Put the id back.
  let selectors = ruleView.doc.querySelectorAll(".ruleview-selector");
  is(selectors.length, 3, "Three rules visible.");
  is(selectors[0].textContent.indexOf("element"), 0, "First item is inline style.");
  is(selectors[1].textContent.indexOf("#testid"), 0, "Second item is id rule.");
  is(selectors[2].textContent.indexOf(".testclass"), 0, "Third item is class rule.");

  testPropertyChanges();
}

function validateTextProp(aProp, aEnabled, aName, aValue, aDesc)
{
  is(aProp.enabled, aEnabled, aDesc + ": enabled.");
  is(aProp.name, aName, aDesc + ": name.");
  is(aProp.value, aValue, aDesc + ": value.");

  is(aProp.editor.enable.hasAttribute("checked"), aEnabled, aDesc + ": enabled checkbox.");
  is(aProp.editor.nameSpan.textContent, aName, aDesc + ": name span.");
  is(aProp.editor.valueSpan.textContent, aValue, aDesc + ": value span.");
}

function testPropertyChanges()
{
  // Add a second margin-top value, just to make things interesting.
  let ruleEditor = ruleView._elementStyle.rules[0].editor;
  ruleEditor.addProperty("margin-top", "5px", "");

  let rule = ruleView._elementStyle.rules[0];

  // Set the element style back to a 1px margin-top.
  testElement.setAttribute("style", "margin-top: 1px; padding-top: 5px");
  ruleView.nodeChanged();
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3, "Correct number of properties");
  validateTextProp(rule.textProps[0], true, "margin-top", "1px", "First margin property re-enabled");
  validateTextProp(rule.textProps[2], false, "margin-top", "5px", "Second margin property disabled");

  // Now set it back to 5px, the 5px value should be re-enabled.
  testElement.setAttribute("style", "margin-top: 5px; padding-top: 5px;");
  ruleView.nodeChanged();
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3, "Correct number of properties");
  validateTextProp(rule.textProps[0], false, "margin-top", "1px", "First margin property re-enabled");
  validateTextProp(rule.textProps[2], true, "margin-top", "5px", "Second margin property disabled");

  // Set the margin property to a value that doesn't exist in the editor.
  // Should reuse the currently-enabled element (the second one.)
  testElement.setAttribute("style", "margin-top: 15px; padding-top: 5px;");
  ruleView.nodeChanged();
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3, "Correct number of properties");
  validateTextProp(rule.textProps[0], false, "margin-top", "1px", "First margin property re-enabled");
  validateTextProp(rule.textProps[2], true, "margin-top", "15px", "Second margin property disabled");

  // Remove the padding-top attribute.  Should disable the padding property but not remove it.
  testElement.setAttribute("style", "margin-top: 5px;");
  ruleView.nodeChanged();
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3, "Correct number of properties");
  validateTextProp(rule.textProps[1], false, "padding-top", "5px", "Padding property disabled");

  // Put the padding-top attribute back in, should re-enable the padding property.
  testElement.setAttribute("style", "margin-top: 5px; padding-top: 25px");
  ruleView.nodeChanged();
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3, "Correct number of properties");
  validateTextProp(rule.textProps[1], true, "padding-top", "25px", "Padding property enabled");

  // Add an entirely new property.
  testElement.setAttribute("style", "margin-top: 5px; padding-top: 25px; padding-left: 20px;");
  ruleView.nodeChanged();
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 4, "Added a property");
  validateTextProp(rule.textProps[3], true, "padding-left", "20px", "Padding property enabled");

  finishTest();
}

function finishTest()
{
  ruleView.clear();
  ruleDialog.close();
  ruleDialog = ruleView = null;
  doc = null;
  gBrowser.removeCurrentTab();
  finish();
}

function test()
{
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, arguments.callee, true);
    doc = content.document;
    waitForFocus(startTest, content);
  }, true);

  content.location = "data:text/html,basic style inspector tests";
}
