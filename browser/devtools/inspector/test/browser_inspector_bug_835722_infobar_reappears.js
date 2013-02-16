/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let inspector;

  function startLocationTests() {
    openInspector(runInspectorTests);
  }

  function runInspectorTests(aInspector) {
    inspector = aInspector;

    executeSoon(function() {
      inspector.selection.once("new-node", onNewSelection);
      info("selecting the DOCTYPE node");
      inspector.selection.setNode(content.document.doctype, "test");
    });
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
      mouseOutAndContinue();
    }, false);
    EventUtils.synthesizeMouse(ruleView, 10, 50, {type: "mouseover"},
                               ruleView.ownerDocument.defaultView);
  }

  function mouseOutAndContinue() {
    let ruleView = inspector.sidebar.getTab("ruleview");
    ruleView.addEventListener("mouseout", function onMouseOut() {
      ruleView.removeEventListener("mouseout", onMouseOut, false);
      is(inspector.highlighter.isHidden(), true,
         "The infobar should not be visible after we mouseout of rules view");
      switchToWebConsole();
    }, false);
    EventUtils.synthesizeMouse(ruleView, 10, 10, {type: "mousemove"},
                               ruleView.ownerDocument.defaultView);
    EventUtils.synthesizeMouse(ruleView, -10, -10, {type: "mouseout"},
                               ruleView.ownerDocument.defaultView);
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
