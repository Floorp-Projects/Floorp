/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let instance;

  let ruleView;
  let inspector;
  let mgr = ResponsiveUI.ResponsiveUIManager;

  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    waitForFocus(startTest, content);
  }, true);

  content.location = "data:text/html;charset=utf-8,<html><style>" +
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

    openRuleView().then(onInspectorUIOpen);
  }

  function onInspectorUIOpen(args) {
    inspector = args.inspector;
    ruleView = args.view;
    ok(inspector, "Got inspector instance");

    let div = content.document.getElementsByTagName("div")[0];
    inspector.selection.setNode(div);
    inspector.once("inspector-updated", testShrink);
  }

  function testShrink() {

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
      testEscapeOpensSplitConsole();
    }, false);

    instance.setSize(500, 500);
  }

  function testEscapeOpensSplitConsole() {
    is(document.getElementById("Tools:ResponsiveUI").getAttribute("checked"), "true", "menu checked");
    ok(!inspector._toolbox._splitConsole, "Console is not split.");

    inspector._toolbox.once("split-console", function() {
      mgr.once("off", function() {executeSoon(finishUp)});
      mgr.toggle(window, gBrowser.selectedTab);
    });
    EventUtils.synthesizeKey("VK_ESCAPE", {});
  }

  function finishUp() {
    ok(inspector._toolbox._splitConsole, "Console is split after pressing escape.");

    // Menus are correctly updated?
    is(document.getElementById("Tools:ResponsiveUI").getAttribute("checked"), "false", "menu unchecked");

    gBrowser.removeCurrentTab();
    finish();
  }
}
