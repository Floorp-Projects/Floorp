/* Any copyright", " is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let contentTab, contentDoc, inspector;

  // Create a tab
  contentTab = gBrowser.selectedTab = gBrowser.addTab();

  // Open the toolbox's inspector first
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
    inspector = toolbox.getCurrentPanel();

    inspector.selection.setNode(content.document.querySelector("body"));
    inspector.once("inspector-updated", () => {
      is(inspector.selection.node.tagName, "BODY", "Inspector displayed");

      // Then load our test page
      gBrowser.selectedBrowser.addEventListener("load", function onload() {
        gBrowser.selectedBrowser.removeEventListener("load", onload, true);
        contentDoc = content.document;

        // The content doc contains a script that creates an iframe and deletes
        // it immediately after. This is what used to make the inspector go
        // blank when navigating to that page.

        var iframe = contentDoc.createElement("iframe");
        contentDoc.body.appendChild(iframe);
        iframe.parentNode.removeChild(iframe);

        is(contentDoc.querySelector("iframe"), null, "Iframe has been removed");

        inspector.once("markuploaded", () => {
          // Assert that the markup-view is displayed and works
          is(contentDoc.querySelector("iframe"), null, "Iframe has been removed");
          is(contentDoc.getElementById("yay").textContent, "load", "Load event fired");

          inspector.selection.setNode(contentDoc.getElementById("yay"));
          inspector.once("inspector-updated", () => {
            ok(inspector.selection.node, "Inspector still displayed");

            // Clean up and go
            contentTab, contentDoc, inspector = null;
            gBrowser.removeTab(contentTab);
            finish();
          });
        });
      }, true);
      content.location = "http://mochi.test:8888/browser/browser/devtools/" +
       "inspector/test/browser_inspector_dead_node_exception.html";
    });
  });
}
