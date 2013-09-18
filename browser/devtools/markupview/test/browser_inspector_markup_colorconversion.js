/* Any copyright", " is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let promise = Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js").Promise;
  let inspector;
  let doc;

  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html,browser_inspector_markupview_colorconversion.js";

  function createDocument() {
    doc.body.innerHTML = '' +
    '<span style="color:red; border-radius:10px; ' +
    'background-color:rgba(0, 255, 0, 1); display: inline-block;">' +
    'Some styled text</span>';

    doc.title = "Style Inspector key binding test";

    setupTest();
  }

  function setupTest() {
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
      inspector = toolbox.getCurrentPanel();
      inspector.once("inspector-updated", checkColors);
    });
  }

  function checkColors() {
    let node = content.document.querySelector("span");
    assertAttributes(node, {
      style: "color:#F00; border-radius:10px; background-color:#0F0; display: inline-block;"
    }).then(() => {
      finishUp();
    });
  }

  // This version of assertAttributes is different from that in other markup
  // view tests. This is because in most tests we are checking the node
  // attributes but here we need the actual values displayed in the markup panel.
  function assertAttributes(node, attributes) {
    let deferred = promise.defer();
    let attrsToCheck = Object.getOwnPropertyNames(attributes);
    ok(node, "captain, we have the node");

    let checkAttrs = function() {
      let container = inspector.markup._selectedContainer;
      let nodeAttrs = container.editor.attrs;

      is(node.attributes.length, attrsToCheck.length,
        "Node has the correct number of attributes");

      for (let attr of attrsToCheck) {
        ok(nodeAttrs[attr], "Node has a " + attr + "attribute");

        let nodeAttributeText = nodeAttrs[attr].textContent;
        [, nodeAttributeText] = nodeAttributeText.match(/^\s*[\w-]+\s*=\s*"(.*)"$/);
        is(nodeAttributeText, attributes[attr],
          "Node has the correct " + attr + " attribute value.");
      }
      deferred.resolve();
    };

    if (inspector.selection.node == node) {
      checkAttrs();
    } else {
      inspector.once("inspector-updated", () => {
        checkAttrs();
      });
      inspector.selection.setNode(node);
    }
    return deferred.promise;
  }

  function finishUp() {
    doc = inspector = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
