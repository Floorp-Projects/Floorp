/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let doc;
let inspector;
let view;

const TEST_URI = "http://example.com/browser/browser/" +
                 "devtools/styleinspector/test/" +
                 "browser_ruleview_pseudoelement.html";

function testPseudoElements(aInspector, aRuleView)
{
  inspector = aInspector;
  view = aRuleView;

  testTopLeft();
}

function testTopLeft()
{
  testNode(doc.querySelector("#topleft"), (element, elementStyle) => {
    let elementRules = elementStyle.rules.filter((rule) => { return !rule.pseudoElement; });
    let afterRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":after"; });
    let beforeRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":before"; });
    let firstLineRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":first-line"; });
    let firstLetterRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":first-letter"; });
    let selectionRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":-moz-selection"; });

    is(elementRules.length, 4, "TopLeft has the correct number of non psuedo element rules");
    is(afterRules.length, 1, "TopLeft has the correct number of :after rules");
    is(beforeRules.length, 2, "TopLeft has the correct number of :before rules");
    is(firstLineRules.length, 0, "TopLeft has the correct number of :first-line rules");
    is(firstLetterRules.length, 0, "TopLeft has the correct number of :first-letter rules");
    is(selectionRules.length, 0, "TopLeft has the correct number of :selection rules");

    let gutters = view.element.querySelectorAll(".theme-gutter");
    is (gutters.length, 3, "There are three gutter headings");
    is (gutters[0].textContent, "Pseudo-elements", "Gutter heading is correct");
    is (gutters[1].textContent, "This Element", "Gutter heading is correct");
    is (gutters[2].textContent, "Inherited from body", "Gutter heading is correct");

    // Make sure that clicking on the twisty hides pseudo elements
    let expander = gutters[0].querySelector(".ruleview-expander");
    ok (view.element.classList.contains("show-pseudo-elements"), "Pseudo Elements are expanded");
    expander.click();
    ok (!view.element.classList.contains("show-pseudo-elements"), "Pseudo Elements are collapsed by twisty");
    expander.click();
    ok (view.element.classList.contains("show-pseudo-elements"), "Pseudo Elements are expanded again");

    // Make sure that dblclicking on the header container also toggles the pseudo elements
    EventUtils.synthesizeMouseAtCenter(gutters[0], {clickCount: 2}, inspector.sidebar.getWindowForTab("ruleview"));
    ok (!view.element.classList.contains("show-pseudo-elements"), "Pseudo Elements are collapsed by dblclicking");

    let defaultView = element.ownerDocument.defaultView;
    let elementRule = elementRules[0];
    let elementRuleView = [].filter.call(view.element.children, (e) => {
      return e._ruleEditor && e._ruleEditor.rule === elementRule;
    })[0]._ruleEditor;

    let elementAfterRule = afterRules[0];
    let elementAfterRuleView = [].filter.call(view.element.children, (e) => {
      return e._ruleEditor && e._ruleEditor.rule === elementAfterRule;
    })[0]._ruleEditor;

    is
    (
      convertTextPropsToString(elementAfterRule.textProps),
      "background: none repeat scroll 0% 0% red; content: \" \"; position: absolute; " +
      "border-radius: 50%; height: 32px; width: 32px; top: 50%; left: 50%; margin-top: -16px; margin-left: -16px",
      "TopLeft after properties are correct"
    );

    let elementBeforeRule = beforeRules[0];
    let elementBeforeRuleView = [].filter.call(view.element.children, (e) => {
      return e._ruleEditor && e._ruleEditor.rule === elementBeforeRule;
    })[0]._ruleEditor;

    is
    (
      convertTextPropsToString(elementBeforeRule.textProps),
      "top: 0px; left: 0px",
      "TopLeft before properties are correct"
    );

    let firstProp = elementAfterRuleView.addProperty("background-color", "rgb(0, 255, 0)", "");
    let secondProp = elementAfterRuleView.addProperty("padding", "100px", "");

    is (firstProp, elementAfterRule.textProps[elementAfterRule.textProps.length - 2],
        "First added property is on back of array");
    is (secondProp, elementAfterRule.textProps[elementAfterRule.textProps.length - 1],
        "Second added property is on back of array");

    promiseDone(elementAfterRule._applyingModifications.then(() => {
      is(defaultView.getComputedStyle(element, ":after").getPropertyValue("background-color"),
        "rgb(0, 255, 0)", "Added property should have been used.");
      is(defaultView.getComputedStyle(element, ":after").getPropertyValue("padding-top"),
        "100px", "Added property should have been used.");
      is(defaultView.getComputedStyle(element).getPropertyValue("padding-top"),
        "32px", "Added property should not apply to element");

      secondProp.setEnabled(false);

      return elementAfterRule._applyingModifications;
    }).then(() => {
      is(defaultView.getComputedStyle(element, ":after").getPropertyValue("padding-top"), "0px",
        "Disabled property should have been used.");
      is(defaultView.getComputedStyle(element).getPropertyValue("padding-top"), "32px",
        "Added property should not apply to element");

      secondProp.setEnabled(true);

      return elementAfterRule._applyingModifications;
    }).then(() => {
      is(defaultView.getComputedStyle(element, ":after").getPropertyValue("padding-top"), "100px",
        "Enabled property should have been used.");
      is(defaultView.getComputedStyle(element).getPropertyValue("padding-top"), "32px",
        "Added property should not apply to element");

      let firstProp = elementRuleView.addProperty("background-color", "rgb(0, 0, 255)", "");

      return elementRule._applyingModifications;
    }).then(() => {
      is(defaultView.getComputedStyle(element).getPropertyValue("background-color"), "rgb(0, 0, 255)",
        "Added property should have been used.");
      is(defaultView.getComputedStyle(element, ":after").getPropertyValue("background-color"), "rgb(0, 255, 0)",
        "Added prop does not apply to pseudo");

      testTopRight();
    }));
  });
}

function testTopRight()
{
  testNode(doc.querySelector("#topright"), (element, elementStyle) => {

    let elementRules = elementStyle.rules.filter((rule) => { return !rule.pseudoElement; });
    let afterRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":after"; });
    let beforeRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":before"; });
    let firstLineRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":first-line"; });
    let firstLetterRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":first-letter"; });
    let selectionRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":-moz-selection"; });

    is(elementRules.length, 4, "TopRight has the correct number of non psuedo element rules");
    is(afterRules.length, 1, "TopRight has the correct number of :after rules");
    is(beforeRules.length, 2, "TopRight has the correct number of :before rules");
    is(firstLineRules.length, 0, "TopRight has the correct number of :first-line rules");
    is(firstLetterRules.length, 0, "TopRight has the correct number of :first-letter rules");
    is(selectionRules.length, 0, "TopRight has the correct number of :selection rules");

    let gutters = view.element.querySelectorAll(".theme-gutter");
    is (gutters.length, 3, "There are three gutter headings");
    is (gutters[0].textContent, "Pseudo-elements", "Gutter heading is correct");
    is (gutters[1].textContent, "This Element", "Gutter heading is correct");
    is (gutters[2].textContent, "Inherited from body", "Gutter heading is correct");

    let expander = gutters[0].querySelector(".ruleview-expander");
    ok (!view.element.classList.contains("show-pseudo-elements"), "Pseudo Elements remain collapsed after switching element");
    expander.scrollIntoView();
    expander.click();
    ok (view.element.classList.contains("show-pseudo-elements"), "Pseudo Elements are shown again after clicking twisty");

    testBottomRight();
  });
}

function testBottomRight()
{
  testNode(doc.querySelector("#bottomright"), (element, elementStyle) => {

    let elementRules = elementStyle.rules.filter((rule) => { return !rule.pseudoElement; });
    let afterRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":after"; });
    let beforeRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":before"; });
    let firstLineRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":first-line"; });
    let firstLetterRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":first-letter"; });
    let selectionRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":-moz-selection"; });

    is(elementRules.length, 4, "BottomRight has the correct number of non psuedo element rules");
    is(afterRules.length, 1, "BottomRight has the correct number of :after rules");
    is(beforeRules.length, 3, "BottomRight has the correct number of :before rules");
    is(firstLineRules.length, 0, "BottomRight has the correct number of :first-line rules");
    is(firstLetterRules.length, 0, "BottomRight has the correct number of :first-letter rules");
    is(selectionRules.length, 0, "BottomRight has the correct number of :selection rules");

    testBottomLeft();
  });
}

function testBottomLeft()
{
  testNode(doc.querySelector("#bottomleft"), (element, elementStyle) => {

    let elementRules = elementStyle.rules.filter((rule) => { return !rule.pseudoElement; });
    let afterRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":after"; });
    let beforeRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":before"; });
    let firstLineRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":first-line"; });
    let firstLetterRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":first-letter"; });
    let selectionRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":-moz-selection"; });

    is(elementRules.length, 4, "BottomLeft has the correct number of non psuedo element rules");
    is(afterRules.length, 1, "BottomLeft has the correct number of :after rules");
    is(beforeRules.length, 2, "BottomLeft has the correct number of :before rules");
    is(firstLineRules.length, 0, "BottomLeft has the correct number of :first-line rules");
    is(firstLetterRules.length, 0, "BottomLeft has the correct number of :first-letter rules");
    is(selectionRules.length, 0, "BottomLeft has the correct number of :selection rules");

    testParagraph();
  });
}

function testParagraph()
{
  testNode(doc.querySelector("#bottomleft p"), (element, elementStyle) => {

    let elementRules = elementStyle.rules.filter((rule) => { return !rule.pseudoElement; });
    let afterRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":after"; });
    let beforeRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":before"; });
    let firstLineRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":first-line"; });
    let firstLetterRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":first-letter"; });
    let selectionRules = elementStyle.rules.filter((rule) => { return rule.pseudoElement === ":-moz-selection"; });

    is(elementRules.length, 3, "Paragraph has the correct number of non psuedo element rules");
    is(afterRules.length, 0, "Paragraph has the correct number of :after rules");
    is(beforeRules.length, 0, "Paragraph has the correct number of :before rules");
    is(firstLineRules.length, 1, "Paragraph has the correct number of :first-line rules");
    is(firstLetterRules.length, 1, "Paragraph has the correct number of :first-letter rules");
    is(selectionRules.length, 1, "Paragraph has the correct number of :selection rules");

    let gutters = view.element.querySelectorAll(".theme-gutter");
    is (gutters.length, 3, "There are three gutter headings");
    is (gutters[0].textContent, "Pseudo-elements", "Gutter heading is correct");
    is (gutters[1].textContent, "This Element", "Gutter heading is correct");
    is (gutters[2].textContent, "Inherited from body", "Gutter heading is correct");

    let elementFirstLineRule = firstLineRules[0];
    let elementFirstLineRuleView = [].filter.call(view.element.children, (e) => {
      return e._ruleEditor && e._ruleEditor.rule === elementFirstLineRule;
    })[0]._ruleEditor;

    is
    (
      convertTextPropsToString(elementFirstLineRule.textProps),
      "background: none repeat scroll 0% 0% blue",
      "Paragraph first-line properties are correct"
    );

    let elementFirstLetterRule = firstLetterRules[0];
    let elementFirstLetterRuleView = [].filter.call(view.element.children, (e) => {
      return e._ruleEditor && e._ruleEditor.rule === elementFirstLetterRule;
    })[0]._ruleEditor;

    is
    (
      convertTextPropsToString(elementFirstLetterRule.textProps),
      "color: red; font-size: 130%",
      "Paragraph first-letter properties are correct"
    );

    let elementSelectionRule = selectionRules[0];
    let elementSelectionRuleView = [].filter.call(view.element.children, (e) => {
      return e._ruleEditor && e._ruleEditor.rule === elementSelectionRule;
    })[0]._ruleEditor;

    is
    (
      convertTextPropsToString(elementSelectionRule.textProps),
      "color: white; background: none repeat scroll 0% 0% black",
      "Paragraph first-letter properties are correct"
    );

    testBody();
  });
}

function testBody() {

  testNode(doc.querySelector("body"), (element, elementStyle) => {

    let gutters = view.element.querySelectorAll(".theme-gutter");
    is (gutters.length, 0, "There are no gutter headings");

    finishTest();
  });

}
function convertTextPropsToString(textProps) {
  return textProps.map((t) => {
    return t.name + ": " + t.value;
  }).join("; ");
}

function testNode(node, cb)
{
  inspector.once("inspector-updated", () => {
    cb(node, view._elementStyle)
  });
  inspector.selection.setNode(node);
}

function finishTest()
{
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
    waitForFocus(() => openRuleView(testPseudoElements), content);
  }, true);

  content.location = TEST_URI;
}
