/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let inspector, utils;

  function startLocationTests() {
    openInspector(runInspectorTests);
  }

  function runInspectorTests(aInspector) {
    inspector = aInspector;
    utils = inspector.panelWin
                     .QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDOMWindowUtils);
    ok(utils, "utils is defined");
    executeSoon(function() {
      inspector.selection.once("new-node", onNewSelection);
      info("selecting the DOCTYPE node");
      inspector.selection.setNode(content.document.doctype, "test");
    });
  }

  function sendMouseEvent(node, type, x, y) {
    let rect = node.getBoundingClientRect();
    let left = rect.left + x;
    let top = rect.top + y;
    utils.sendMouseEventToWindow(type, left, top, 0, 1, 0, false, 0, 0);
  }

  function onNewSelection() {
    is(inspector.highlighter.isHidden(), true,
       "The infobar should be hidden now on selecting a non element node.");
    inspector.sidebar.select("ruleview");
    let ruleView = inspector.sidebar.getTab("ruleview");
    ruleView.addEventListener("mouseover", function onMouseOver() {
      ruleView.removeEventListener("mouseover", onMouseOver, false);
      is(inspector.highlighter.isHidden(), true,
         "The infobar was hidden so mouseover on the rules view did nothing");
      executeSoon(mouseOutAndContinue);
    }, false);
    sendMouseEvent(ruleView, "mouseover", 10, 10);
  }

  function mouseOutAndContinue() {
    let ruleView = inspector.sidebar.getTab("ruleview");
    info("adding mouseout listener");
    ruleView.addEventListener("mouseout", function onMouseOut() {
      info("mouseout happened");
      ruleView.removeEventListener("mouseout", onMouseOut, false);
      is(inspector.highlighter.isHidden(), true,
         "The infobar should not be visible after we mouseout of rules view");
      switchToWebConsole();
    }, false);
    info("Synthesizing mouseout on " + ruleView);
    sendMouseEvent(inspector._markupBox, "mousemove", 50, 50);
    info("mouseout synthesized");
  }

  function switchToWebConsole() {
    inspector.selection.once("new-node", function() {
      is(inspector.highlighter.isHidden(), false,
         "The infobar should be visible after we select a div.");
      gDevTools.showToolbox(inspector.target, "webconsole").then(function() {
        is(inspector.highlighter.isHidden(), true,
           "The infobar should not be visible after we switched to webconsole");
        reloadAndWait();
      });
    });
    inspector.selection.setNode(content.document.querySelector("div"), "test");
  }

  function reloadAndWait() {
    gBrowser.selectedBrowser.addEventListener("load", function onBrowserLoad() {
      gBrowser.selectedBrowser.removeEventListener("load", onBrowserLoad, true);
      waitForFocus(testAfterReload, content);
    }, true);
    content.location.reload();
  }

  function testAfterReload() {
    is(inspector.highlighter.isHidden(), true,
       "The infobar should not be visible after we reload with webconsole shown");
    testEnd();
  }

  function testEnd() {
    gBrowser.removeCurrentTab();
    utils = null;
    executeSoon(finish);
  }

  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onBrowserLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onBrowserLoad, true);
    waitForFocus(startLocationTests, content);
  }, true);

  content.location = "data:text/html,<!DOCTYPE html><div>Infobar should not " +
                     "reappear</div><p>init</p>";
}
