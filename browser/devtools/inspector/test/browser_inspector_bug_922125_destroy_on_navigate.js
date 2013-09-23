/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let Toolbox = devtools.Toolbox;
let TargetFactory = devtools.TargetFactory;

function test() {
  waitForExplicitFinish();

  const URL_1 = "data:text/html;charset=UTF-8,<div id='one' style='color:red;'>ONE</div>";
  const URL_2 = "data:text/html;charset=UTF-8,<div id='two' style='color:green;'>TWO</div>";

  let inspector;

  // open tab, load URL_1, and wait for load to finish
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let browser = gBrowser.getBrowserForTab(tab);

  function onPageOneLoad() {
    browser.removeEventListener("load", onPageOneLoad, true);

    gDevTools.showToolbox(target).then(aToolbox => {
      return aToolbox.selectTool("inspector");
    }).then(i => {
      inspector = i;

      // Verify we are on page one
      let testNode = content.document.querySelector("#one");
      ok(testNode, "We have the test node on page 1");

      assertMarkupViewIsLoaded();

      // Listen to will-navigate to check if the view is empty
      target.on("will-navigate", () => {
        info("Navigation to page 2 has started, the inspector should be empty");
        assertMarkupViewIsEmpty();
      });
      inspector.once("markuploaded", () => {
        info("Navigation to page 2 was done, the inspector should be back up");

        // Verify we are on page one
        let testNode = content.document.querySelector("#two");
        ok(testNode, "We have the test node on page 2");

        // On page 2 load, verify we have the right content
        assertMarkupViewIsLoaded();
        endTests();
      });

      // Navigate to page 2
      browser.loadURI(URL_2);
    });
  }

  // Navigate to page 1
  browser.addEventListener("load", onPageOneLoad, true);
  browser.loadURI(URL_1);

  function assertMarkupViewIsLoaded() {
    let markupViewBox = inspector.panelDoc.getElementById("markup-box");
    is(markupViewBox.childNodes.length, 1, "The markup-view is loaded");
  }

  function assertMarkupViewIsEmpty() {
    let markupViewBox = inspector.panelDoc.getElementById("markup-box");
    is(markupViewBox.childNodes.length, 0, "The markup-view is unloaded");
  }

  function endTests() {
    target = browser = tab = inspector = TargetFactory = Toolbox = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
