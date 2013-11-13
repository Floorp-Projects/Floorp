
function test() {
  let inspector;
  let doc;
  let selector = "#keyboard";
  let oldHTML = '<div id="keyboard"></div>';
  let newHTML = '<div id="keyboard">Edited</div>';

  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(setupTest, content);
  }, true);

  content.location = "data:text/html," +
    "<!DOCTYPE html>" +
    "<head><meta charset='utf-8' /></head>" +
    "<body>" +
    "<div id=\"keyboard\"></div>" +
    "</body>" +
    "</html>";

  function setupTest() {
    var target = TargetFactory.forTab(gBrowser.selectedTab);
    gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
      inspector = toolbox.getCurrentPanel();
      inspector.once("inspector-updated", startTests);
    });
  }

  function startTests() {
    inspector.markup._frame.focus();
    testEscapeCancels();
  }

  function testEscapeCancels() {
    info("Checking to make sure that pressing escape cancels edits");
    let rawNode = doc.querySelector(selector);

    inspector.selection.once("new-node", () => {

      inspector.markup.htmlEditor.on("popupshown", function onPopupShown() {
        inspector.markup.htmlEditor.off("popupshown", onPopupShown);

        ok (inspector.markup.htmlEditor._visible, "HTML Editor is visible");
        is (rawNode.outerHTML, oldHTML, "The node is starting with old HTML.");

        inspector.markup.htmlEditor.on("popuphidden", function onPopupHidden() {
          inspector.markup.htmlEditor.off("popuphidden", onPopupHidden);
          ok (!inspector.markup.htmlEditor._visible, "HTML Editor is not visible");

          let rawNode = doc.querySelector(selector);
          is (rawNode.outerHTML, oldHTML, "Escape cancels edits");
          testF2Commits();
        });

        inspector.markup.htmlEditor.editor.setText(newHTML);

        EventUtils.sendKey("ESCAPE", inspector.markup.htmlEditor.doc.defaultView);
      });

      EventUtils.sendKey("F2", inspector.markup._frame.contentWindow);
    });

    inspector.selection.setNode(rawNode);
  }

  function testF2Commits() {
    info("Checking to make sure that pressing F2 commits edits");
    let rawNode = doc.querySelector(selector);

    inspector.markup.htmlEditor.on("popupshown", function onPopupShown() {
      inspector.markup.htmlEditor.off("popupshown", onPopupShown);

      ok (inspector.markup.htmlEditor._visible, "HTML Editor is visible");
      is (rawNode.outerHTML, oldHTML, "The node is starting with old HTML.");

      inspector.once("markupmutation", (e, aMutations) => {
        ok (!inspector.markup.htmlEditor._visible, "HTML Editor is not visible");

        let rawNode = doc.querySelector(selector);
        is (rawNode.outerHTML, newHTML, "F2 commits edits - the node has new HTML.");
        testBody();
      });

      inspector.markup.htmlEditor.editor.setText(newHTML);
      EventUtils.sendKey("F2", inspector.markup._frame.contentWindow);
    });

    inspector.markup._frame.contentDocument.documentElement.focus();
    EventUtils.sendKey("F2", inspector.markup._frame.contentWindow);
  }

  function testBody() {
    info("Checking to make sure that editing the <body> element works like other nodes");
    let body = doc.querySelector("body");
    let bodyHTML = '<body id="updated"><p></p></body>';
    let bodyFront = inspector.markup.walker.frontForRawNode(body);
    inspector.once("markupmutation", (e, aMutations) => {
      is (doc.querySelector("body").outerHTML, bodyHTML, "<body> HTML has been updated");
      is (doc.querySelectorAll("head").length, 1, "no extra <head>s have been added");
      testHead();
    });
    inspector.markup.updateNodeOuterHTML(bodyFront, bodyHTML, body.outerHTML);
  }

  function testHead() {
    info("Checking to make sure that editing the <head> element works like other nodes");
    let head = doc.querySelector("head");
    let headHTML = '<head id="updated"><title>New Title</title><script>window.foo="bar";</script></head>';
    let headFront = inspector.markup.walker.frontForRawNode(head);
    inspector.once("markupmutation", (e, aMutations) => {
      is (doc.title, "New Title", "New title has been added");
      is (doc.defaultView.foo, undefined, "Script has not been executed");
      is (doc.querySelector("head").outerHTML, headHTML, "<head> HTML has been updated");
      is (doc.querySelectorAll("body").length, 1, "no extra <body>s have been added");
      testDocumentElement();
    });
    inspector.markup.updateNodeOuterHTML(headFront, headHTML, head.outerHTML);
  }

  function testDocumentElement() {
    info("Checking to make sure that editing the <html> element works like other nodes");
    let docElement = doc.documentElement;
    let docElementHTML = '<html id="updated" foo="bar"><head><title>Updated from document element</title><script>window.foo="bar";</script></head><body><p>Hello</p></body></html>';
    let docElementFront = inspector.markup.walker.frontForRawNode(docElement);
    inspector.once("markupmutation", (e, aMutations) => {
      is (doc.title, "Updated from document element", "New title has been added");
      is (doc.defaultView.foo, undefined, "Script has not been executed");
      is (doc.documentElement.id, "updated", "<html> ID has been updated");
      is (doc.documentElement.className, "", "<html> class has been updated");
      is (doc.documentElement.getAttribute("foo"), "bar", "<html> attribute has been updated");
      is (doc.documentElement.outerHTML, docElementHTML, "<html> HTML has been updated");
      is (doc.querySelectorAll("head").length, 1, "no extra <head>s have been added");
      is (doc.querySelectorAll("body").length, 1, "no extra <body>s have been added");
      is (doc.body.textContent, "Hello", "document.body.textContent has been updated");
      testDocumentElement2();
    });
    inspector.markup.updateNodeOuterHTML(docElementFront, docElementHTML, docElement.outerHTML);
  }

  function testDocumentElement2() {
    info("Checking to make sure (again) that editing the <html> element works like other nodes");
    let docElement = doc.documentElement;
    let docElementHTML = '<html class="updated" id="somethingelse"><head><title>Updated again from document element</title><script>window.foo="bar";</script></head><body><p>Hello again</p></body></html>';
    let docElementFront = inspector.markup.walker.frontForRawNode(docElement);
    inspector.once("markupmutation", (e, aMutations) => {
      is (doc.title, "Updated again from document element", "New title has been added");
      is (doc.defaultView.foo, undefined, "Script has not been executed");
      is (doc.documentElement.id, "somethingelse", "<html> ID has been updated");
      is (doc.documentElement.className, "updated", "<html> class has been updated");
      is (doc.documentElement.getAttribute("foo"), null, "<html> attribute has been removed");
      is (doc.documentElement.outerHTML, docElementHTML, "<html> HTML has been updated");
      is (doc.querySelectorAll("head").length, 1, "no extra <head>s have been added");
      is (doc.querySelectorAll("body").length, 1, "no extra <body>s have been added");
      is (doc.body.textContent, "Hello again", "document.body.textContent has been updated");
      finishUp();
    });
    inspector.markup.updateNodeOuterHTML(docElementFront, docElementHTML, docElement.outerHTML);
  }

  function finishUp() {
    doc = inspector = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
