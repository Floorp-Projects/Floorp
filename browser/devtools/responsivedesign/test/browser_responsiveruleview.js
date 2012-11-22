/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let instance;

  let ruleView;
  let inspector;

  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    waitForFocus(startTest, content);
  }, true);

  content.location = "data:text/html,<html><style>" +
    "div {" +
    "  width: 500px;" +
    "  height: 10px;" +
    "  background: purple;" +
    "} " +
    "@media screen and (max-width: 200px) {" +
    "  div { " +
    "    width: 100px;" +
    "  }" +
    "};" +
    "</style><div></div></html>"

  function numberOfRules() {
    return ruleView.element.querySelectorAll(".ruleview-code").length;
  }

  function startTest() {
    document.getElementById("Tools:ResponsiveUI").doCommand();
    executeSoon(onUIOpen);
  }

  function onUIOpen() {
    instance = gBrowser.selectedTab.__responsiveUI;
    ok(instance, "instance of the module is attached to the tab.");

    instance.stack.setAttribute("notransition", "true");
    registerCleanupFunction(function() {
      instance.stack.removeAttribute("notransition");
    });

    instance.setSize(500, 500);

    openInspector(onInspectorUIOpen);
  }

  function onInspectorUIOpen(aInspector) {
    inspector = aInspector;
    ok(inspector, "Got inspector instance");
    inspector.sidebar.select("ruleview");

    let div = content.document.getElementsByTagName("div")[0];

    inspector.sidebar.once("ruleview-ready", function() {
      Services.obs.addObserver(testShrink, "StyleInspector-populated", false);
      inspector.selection.setNode(div);
    });
  }

  function testShrink() {
    Services.obs.removeObserver(testShrink, "StyleInspector-populated");

    ruleView = inspector.sidebar.getWindowForTab("ruleview").ruleview.view;

    is(numberOfRules(), 2, "Should have two rules initially.");

    ruleView.element.addEventListener("CssRuleViewRefreshed", function refresh() {
      ruleView.element.removeEventListener("CssRuleViewRefreshed", refresh, false);
      is(numberOfRules(), 3, "Should have three rules after shrinking.");
      testGrow();
    }, false);

    instance.setSize(100, 100);
  }

  function testGrow() {
    ruleView.element.addEventListener("CssRuleViewRefreshed", function refresh() {
      ruleView.element.removeEventListener("CssRuleViewRefreshed", refresh, false);
      is(numberOfRules(), 2, "Should have two rules after growing.");
      finishUp();
    }, false);

    instance.setSize(500, 500);
  }

  function finishUp() {
    document.getElementById("Tools:ResponsiveUI").doCommand();

    // Menus are correctly updated?
    is(document.getElementById("Tools:ResponsiveUI").getAttribute("checked"), "false", "menu unchecked");

    gBrowser.removeCurrentTab();
    finish();
  }
}
