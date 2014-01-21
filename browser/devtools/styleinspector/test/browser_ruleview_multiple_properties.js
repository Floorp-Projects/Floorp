/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let doc;
let ruleWindow;
let ruleView;
let inspector;
let elementRuleEditor;

function startTest()
{
  doc.body.innerHTML = '<h1>Testing Multiple Properties</h1>';

  openRuleView((aInspector, aRuleView) => {
    inspector = aInspector;
    ruleView = aRuleView;
    ruleWindow = aRuleView.doc.defaultView;
    selectNewElement().then(testCreateNewMulti);
  });
}

/*
 * Add a new node to the DOM and resolve the promise once it is ready to use
 */
function selectNewElement()
{
  let newElement = doc.createElement("div");
  newElement.textContent = "Test Element";
  doc.body.appendChild(newElement);
  inspector.selection.setNode(newElement);
  let def = promise.defer();
  ruleView.element.addEventListener("CssRuleViewRefreshed", function changed() {
    ruleView.element.removeEventListener("CssRuleViewRefreshed", changed);
    elementRuleEditor = ruleView.element.children[0]._ruleEditor;
    def.resolve();
  });
  return def.promise;
}

/*
 * Begin the creation of a new property, resolving after the editor
 * has been created.
 */
function beginNewProp()
{
  let def = promise.defer();
  waitForEditorFocus(elementRuleEditor.element, function onNewElement(aEditor) {

    is(inplaceEditor(elementRuleEditor.newPropSpan), aEditor, "Next focused editor should be the new property editor.");
    is(elementRuleEditor.rule.textProps.length,  0, "Should be starting with one new text property.");
    is(elementRuleEditor.propertyList.children.length, 1, "Should be starting with two property editors.");

    def.resolve(aEditor);
  });
  elementRuleEditor.closeBrace.scrollIntoView();
  EventUtils.synthesizeMouse(elementRuleEditor.closeBrace, 1, 1,
                             { },
                             ruleWindow);
  return def.promise;
}

/*
 * Fully create a new property, given some text input
 */
function createNewProp(inputValue)
{
  let def = promise.defer();
  beginNewProp().then((aEditor)=>{
    aEditor.input.value = inputValue;

    waitForEditorFocus(elementRuleEditor.element, function onNewValue(aEditor) {
      promiseDone(expectRuleChange(elementRuleEditor.rule).then(() => {
        def.resolve();
      }));
    });

    EventUtils.synthesizeKey("VK_RETURN", {}, ruleWindow);
  });

  return def.promise;
}

function testCreateNewMulti()
{
  createNewProp(
    "color:blue;background : orange   ; text-align:center; border-color: green;"
  ).then(()=>{
    is(elementRuleEditor.rule.textProps.length, 4, "Should have created a new text property.");
    is(elementRuleEditor.propertyList.children.length, 5, "Should have created a new property editor.");

    is(elementRuleEditor.rule.textProps[0].name, "color", "Should have correct property name");
    is(elementRuleEditor.rule.textProps[0].value, "blue", "Should have correct property value");

    is(elementRuleEditor.rule.textProps[1].name, "background", "Should have correct property name");
    is(elementRuleEditor.rule.textProps[1].value, "orange", "Should have correct property value");

    is(elementRuleEditor.rule.textProps[2].name, "text-align", "Should have correct property name");
    is(elementRuleEditor.rule.textProps[2].value, "center", "Should have correct property value");

    is(elementRuleEditor.rule.textProps[3].name, "border-color", "Should have correct property name");
    is(elementRuleEditor.rule.textProps[3].value, "green", "Should have correct property value");

    selectNewElement().then(testCreateNewMultiDuplicates);
  });
}

function testCreateNewMultiDuplicates()
{
  createNewProp(
    "color:red;color:orange;color:yellow;color:green;color:blue;color:indigo;color:violet;"
  ).then(()=>{
    is(elementRuleEditor.rule.textProps.length, 7, "Should have created new text properties.");
    is(elementRuleEditor.propertyList.children.length, 8, "Should have created new property editors.");

    is(elementRuleEditor.rule.textProps[0].name, "color", "Should have correct property name");
    is(elementRuleEditor.rule.textProps[0].value, "red", "Should have correct property value");

    is(elementRuleEditor.rule.textProps[1].name, "color", "Should have correct property name");
    is(elementRuleEditor.rule.textProps[1].value, "orange", "Should have correct property value");

    is(elementRuleEditor.rule.textProps[2].name, "color", "Should have correct property name");
    is(elementRuleEditor.rule.textProps[2].value, "yellow", "Should have correct property value");

    is(elementRuleEditor.rule.textProps[3].name, "color", "Should have correct property name");
    is(elementRuleEditor.rule.textProps[3].value, "green", "Should have correct property value");

    is(elementRuleEditor.rule.textProps[4].name, "color", "Should have correct property name");
    is(elementRuleEditor.rule.textProps[4].value, "blue", "Should have correct property value");

    is(elementRuleEditor.rule.textProps[5].name, "color", "Should have correct property name");
    is(elementRuleEditor.rule.textProps[5].value, "indigo", "Should have correct property value");

    is(elementRuleEditor.rule.textProps[6].name, "color", "Should have correct property name");
    is(elementRuleEditor.rule.textProps[6].value, "violet", "Should have correct property value");

    selectNewElement().then(testCreateNewMultiPriority);
  });
}

function testCreateNewMultiPriority()
{
  createNewProp(
    "color:red;width:100px;height: 100px;"
  ).then(()=>{
    is(elementRuleEditor.rule.textProps.length, 3, "Should have created new text properties.");
    is(elementRuleEditor.propertyList.children.length, 4, "Should have created new property editors.");

    is(elementRuleEditor.rule.textProps[0].name, "color", "Should have correct property name");
    is(elementRuleEditor.rule.textProps[0].value, "red", "Should have correct property value");

    is(elementRuleEditor.rule.textProps[1].name, "width", "Should have correct property name");
    is(elementRuleEditor.rule.textProps[1].value, "100px", "Should have correct property value");

    is(elementRuleEditor.rule.textProps[2].name, "height", "Should have correct property name");
    is(elementRuleEditor.rule.textProps[2].value, "100px", "Should have correct property value");

    selectNewElement().then(testCreateNewMultiUnfinished);
  });
}

function testCreateNewMultiUnfinished()
{
  createNewProp(
    "color:blue;background : orange   ; text-align:center; border-color: "
  ).then(()=>{
    is(elementRuleEditor.rule.textProps.length, 4, "Should have created new text properties.");
    is(elementRuleEditor.propertyList.children.length, 4, "Should have created property editors.");

    for (let ch of "red") {
      EventUtils.sendChar(ch, ruleWindow);
    }
    EventUtils.synthesizeKey("VK_RETURN", {}, ruleWindow);

    is(elementRuleEditor.rule.textProps.length, 4, "Should have the same number of text properties.");
    is(elementRuleEditor.propertyList.children.length, 5, "Should have added the changed value editor.");

    is(elementRuleEditor.rule.textProps[0].name, "color", "Should have correct property name");
    is(elementRuleEditor.rule.textProps[0].value, "blue", "Should have correct property value");

    is(elementRuleEditor.rule.textProps[1].name, "background", "Should have correct property name");
    is(elementRuleEditor.rule.textProps[1].value, "orange", "Should have correct property value");

    is(elementRuleEditor.rule.textProps[2].name, "text-align", "Should have correct property name");
    is(elementRuleEditor.rule.textProps[2].value, "center", "Should have correct property value");

    is(elementRuleEditor.rule.textProps[3].name, "border-color", "Should have correct property name");
    is(elementRuleEditor.rule.textProps[3].value, "red", "Should have correct property value");

    selectNewElement().then(testCreateNewMultiPartialUnfinished);
  });
}


function testCreateNewMultiPartialUnfinished()
{
  createNewProp(
    "width: 100px; heig"
  ).then(()=>{
    is(elementRuleEditor.rule.textProps.length, 2, "Should have created a new text property.");
    is(elementRuleEditor.propertyList.children.length, 2, "Should have created a property editor.");

    // Value is focused, lets add multiple rules here and make sure they get added
    let valueEditor = elementRuleEditor.propertyList.children[1].querySelector("input");
    valueEditor.value = "10px;background:orangered;color: black;";
    EventUtils.synthesizeKey("VK_RETURN", {}, ruleWindow);

    is(elementRuleEditor.rule.textProps.length, 4, "Should have added the changed value.");
    is(elementRuleEditor.propertyList.children.length, 5, "Should have added the changed value editor.");

    is(elementRuleEditor.rule.textProps[0].name, "width", "Should have correct property name");
    is(elementRuleEditor.rule.textProps[0].value, "100px", "Should have correct property value");

    is(elementRuleEditor.rule.textProps[1].name, "heig", "Should have correct property name");
    is(elementRuleEditor.rule.textProps[1].value, "10px", "Should have correct property value");

    is(elementRuleEditor.rule.textProps[2].name, "background", "Should have correct property name");
    is(elementRuleEditor.rule.textProps[2].value, "orangered", "Should have correct property value");

    is(elementRuleEditor.rule.textProps[3].name, "color", "Should have correct property name");
    is(elementRuleEditor.rule.textProps[3].value, "black", "Should have correct property value");

    selectNewElement().then(testMultiValues);
  });
}

function testMultiValues()
{
  createNewProp(
    "width:"
  ).then(()=>{
    is(elementRuleEditor.rule.textProps.length, 1, "Should have created a new text property.");
    is(elementRuleEditor.propertyList.children.length, 1, "Should have created a property editor.");

    // Value is focused, lets add multiple rules here and make sure they get added
    let valueEditor = elementRuleEditor.propertyList.children[0].querySelector("input");
    valueEditor.value = "height: 10px;color:blue"
    EventUtils.synthesizeKey("VK_RETURN", {}, ruleWindow);

    is(elementRuleEditor.rule.textProps.length, 2, "Should have added the changed value.");
    is(elementRuleEditor.propertyList.children.length, 3, "Should have added the changed value editor.");

    EventUtils.synthesizeKey("VK_ESCAPE", {}, ruleWindow);
    is(elementRuleEditor.propertyList.children.length, 2, "Should have removed the value editor.");

    is(elementRuleEditor.rule.textProps[0].name, "width", "Should have correct property name");
    is(elementRuleEditor.rule.textProps[0].value, "height: 10px", "Should have correct property value");

    is(elementRuleEditor.rule.textProps[1].name, "color", "Should have correct property name");
    is(elementRuleEditor.rule.textProps[1].value, "blue", "Should have correct property value");

    finishTest();
  });
}

function finishTest()
{
  inspector = ruleWindow = ruleView = doc = elementRuleEditor = null;
  gBrowser.removeCurrentTab();
  finish();
}

function test()
{
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function changedValues_load(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, changedValues_load, true);
    doc = content.document;
    waitForFocus(startTest, content);
  }, true);

  content.location = "data:text/html,test rule view user changes";
}
