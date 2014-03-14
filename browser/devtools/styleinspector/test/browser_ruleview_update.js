/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let doc;

let inspector;
let ruleView;
let testElement;
let rule;

function startTest(aInspector, aRuleView)
{
  inspector = aInspector;
  ruleView = aRuleView;
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

  inspector.selection.setNode(testElement);
  inspector.once("rule-view-refreshed", testRuleChanges);
}

function testRuleChanges()
{
  let selectors = ruleView.doc.querySelectorAll(".ruleview-selector");
  is(selectors.length, 3, "Three rules visible.");
  is(selectors[0].textContent.indexOf("element"), 0, "First item is inline style.");
  is(selectors[1].textContent.indexOf("#testid"), 0, "Second item is id rule.");
  is(selectors[2].textContent.indexOf(".testclass"), 0, "Third item is class rule.");

  // Change the id and refresh.
  inspector.once("rule-view-refreshed", testRuleChange1);
  testElement.setAttribute("id", "differentid");
}

function testRuleChange1()
{
  let selectors = ruleView.doc.querySelectorAll(".ruleview-selector");
  is(selectors.length, 2, "Two rules visible.");
  is(selectors[0].textContent.indexOf("element"), 0, "First item is inline style.");
  is(selectors[1].textContent.indexOf(".testclass"), 0, "Second item is class rule.");

  inspector.once("rule-view-refreshed", testRuleChange2);
  testElement.setAttribute("id", "testid");
}
function testRuleChange2()
{
  let selectors = ruleView.doc.querySelectorAll(".ruleview-selector");
  is(selectors.length, 3, "Three rules visible.");
  is(selectors[0].textContent.indexOf("element"), 0, "First item is inline style.");
  is(selectors[1].textContent.indexOf("#testid"), 0, "Second item is id rule.");
  is(selectors[2].textContent.indexOf(".testclass"), 0, "Third item is class rule.");

  testPropertyChanges();
}

function validateTextProp(aProp, aEnabled, aName, aValue, aDesc, valueSpanText)
{
  is(aProp.enabled, aEnabled, aDesc + ": enabled.");
  is(aProp.name, aName, aDesc + ": name.");
  is(aProp.value, aValue, aDesc + ": value.");

  is(aProp.editor.enable.hasAttribute("checked"), aEnabled, aDesc + ": enabled checkbox.");
  is(aProp.editor.nameSpan.textContent, aName, aDesc + ": name span.");
  is(aProp.editor.valueSpan.textContent, valueSpanText || aValue, aDesc + ": value span.");
}

function testPropertyChanges()
{
  rule = ruleView._elementStyle.rules[0];
  let ruleEditor = ruleView._elementStyle.rules[0].editor;
  inspector.once("rule-view-refreshed", testPropertyChange0);

  // Add a second margin-top value, just to make things interesting.
  ruleEditor.addProperty("margin-top", "5px", "");
}

function testPropertyChange0()
{
  validateTextProp(rule.textProps[0], false, "margin-top", "1px", "Original margin property active");

  inspector.once("rule-view-refreshed", testPropertyChange1);
  testElement.setAttribute("style", "margin-top: 1px; padding-top: 5px");
}
function testPropertyChange1()
{
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3, "Correct number of properties");
  validateTextProp(rule.textProps[0], true, "margin-top", "1px", "First margin property re-enabled");
  validateTextProp(rule.textProps[2], false, "margin-top", "5px", "Second margin property disabled");

  inspector.once("rule-view-refreshed", testPropertyChange2);

  // Now set it back to 5px, the 5px value should be re-enabled.
  testElement.setAttribute("style", "margin-top: 5px; padding-top: 5px;");
}
function testPropertyChange2()
{
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3, "Correct number of properties");
  validateTextProp(rule.textProps[0], false, "margin-top", "1px", "First margin property re-enabled");
  validateTextProp(rule.textProps[2], true, "margin-top", "5px", "Second margin property disabled");

  inspector.once("rule-view-refreshed", testPropertyChange3);

  // Set the margin property to a value that doesn't exist in the editor.
  // Should reuse the currently-enabled element (the second one.)
  testElement.setAttribute("style", "margin-top: 15px; padding-top: 5px;");
}
function testPropertyChange3()
{
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3, "Correct number of properties");
  validateTextProp(rule.textProps[0], false, "margin-top", "1px", "First margin property re-enabled");
  validateTextProp(rule.textProps[2], true, "margin-top", "15px", "Second margin property disabled");

  inspector.once("rule-view-refreshed", testPropertyChange4);

  // Remove the padding-top attribute.  Should disable the padding property but not remove it.
  testElement.setAttribute("style", "margin-top: 5px;");
}
function testPropertyChange4()
{
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3, "Correct number of properties");
  validateTextProp(rule.textProps[1], false, "padding-top", "5px", "Padding property disabled");

  inspector.once("rule-view-refreshed", testPropertyChange5);

  // Put the padding-top attribute back in, should re-enable the padding property.
  testElement.setAttribute("style", "margin-top: 5px; padding-top: 25px");
}
function testPropertyChange5()
{
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3, "Correct number of properties");
  validateTextProp(rule.textProps[1], true, "padding-top", "25px", "Padding property enabled");

  inspector.once("rule-view-refreshed", testPropertyChange6);

  // Add an entirely new property
  testElement.setAttribute("style", "margin-top: 5px; padding-top: 25px; padding-left: 20px;");
}
function testPropertyChange6()
{
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 4, "Added a property");
  validateTextProp(rule.textProps[3], true, "padding-left", "20px", "Padding property enabled");

  inspector.once("rule-view-refreshed", testPropertyChange7);

  // Add an entirely new property
  testElement.setAttribute("style", "background: url(\"chrome://branding/content/about-logo.png\") repeat scroll 0% 0% red");
}

function testPropertyChange7()
{
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 5, "Added a property");
  validateTextProp(rule.textProps[4], true, "background",
                   "url(\"chrome://branding/content/about-logo.png\") repeat scroll 0% 0% red",
                   "shortcut property correctly set",
                   "url('chrome://branding/content/about-logo.png') repeat scroll 0% 0% #F00");

  finishTest();
}

function finishTest()
{
  inspector = ruleView = rule = null;
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
    waitForFocus(() => openRuleView(startTest), content);
  }, true);

  content.location = "data:text/html;charset=utf-8,browser_ruleview_update.js";
}
