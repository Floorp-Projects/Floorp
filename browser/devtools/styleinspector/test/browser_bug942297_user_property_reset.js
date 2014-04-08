/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that user set style properties can be changed from the markup-view and
// don't survive page reload

let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let promise = devtools.require("sdk/core/promise");
let {Task} = Cu.import("resource://gre/modules/Task.jsm", {});

let TEST_PAGE = [
  "data:text/html,",
  "<p id='id1' style='width:200px;'>element 1</p>",
  "<p id='id2' style='width:100px;'>element 2</p>"
].join("");

let doc;
let inspector;
let ruleView;
let markupView;

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload(evt) {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(() => {
      openRuleView((aInspector, aView) => {
        inspector = aInspector;
        ruleView = aView;
        markupView = inspector.markup;

        Task.spawn(function() {
          yield selectElement("id1");
          yield modifyRuleViewWidth("300px");
          assertRuleAndMarkupViewWidth("id1", "300px");
          yield selectElement("id2");
          assertRuleAndMarkupViewWidth("id2", "100px");
          yield modifyRuleViewWidth("50px");
          assertRuleAndMarkupViewWidth("id2", "50px");

          yield reloadPage();
          yield selectElement("id1");
          assertRuleAndMarkupViewWidth("id1", "200px");
          yield selectElement("id2");
          assertRuleAndMarkupViewWidth("id2", "100px");

          finishTest();
        }).then(null, Cu.reportError);
      });
    }, content);
  }, true);

  content.location = TEST_PAGE;
}

function finishTest() {
  doc = inspector = ruleView = markupView = null;
  gBrowser.removeCurrentTab();
  finish();
}

function selectElement(id) {
  let deferred = promise.defer();
  inspector.selection.setNode(doc.getElementById(id));
  inspector.once("inspector-updated", deferred.resolve);
  return deferred.promise;
}

function getStyleRule() {
  return ruleView.doc.querySelector(".ruleview-rule");
}

function modifyRuleViewWidth(value) {
  let deferred = promise.defer();

  let valueSpan = getStyleRule().querySelector(".ruleview-propertyvalue");
  waitForEditorFocus(valueSpan.parentNode, () => {
    let editor = inplaceEditor(valueSpan);
    editor.input.value = value;
    waitForEditorBlur(editor, () => {
      // Changing the style will refresh the markup view, let's wait for that
      inspector.once("markupmutation", () => {
        waitForEditorBlur({input: ruleView.doc.activeElement}, deferred.resolve);
        EventUtils.sendKey("escape");
      });
    });
    EventUtils.sendKey("return");
  });
  valueSpan.click();

  return deferred.promise;
}

function getContainerStyleAttrValue(id) {
  let front = markupView.walker.frontForRawNode(doc.getElementById(id));
  let container = markupView.getContainer(front);

  let attrIndex = 0;
  for (let attrName of container.elt.querySelectorAll(".attr-name")) {
    if (attrName.textContent === "style") {
      return container.elt.querySelectorAll(".attr-value")[attrIndex];
    }
    attrIndex ++;
  }
}

function assertRuleAndMarkupViewWidth(id, value) {
  let valueSpan = getStyleRule().querySelector(".ruleview-propertyvalue");
  is(valueSpan.textContent, value, "Rule-view style width is " + value + " as expected");

  let attr = getContainerStyleAttrValue(id);
  is(attr.textContent.replace(/\s/g, ""), "width:" + value + ";", "Markup-view style attribute width is " + value);
}

function reloadPage() {
  let deferred = promise.defer();
  inspector.once("new-root", () => {
    doc = content.document;
    markupView = inspector.markup;
    markupView._waitForChildren().then(deferred.resolve);
  });
  content.location.reload();
  return deferred.promise;
}
