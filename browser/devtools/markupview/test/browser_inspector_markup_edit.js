/* Any copyright", " is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that various editors work as expected. Also checks
 * that the various changes are properly undoable and redoable.
 * For each step in the test, we:
 * - Run the setup for that test (if any)
 * - Check that the node we're editing is as we expect
 * - Make the change, check that the change was made as we expect
 * - Undo the change, check that the node is back in its original state
 * - Redo the change, check that the node change was made again correctly.
 *
 * This test mostly tries to verify that the editor makes changes to the
 * underlying DOM, not that the UI updates - UI updates are based on
 * underlying DOM changes, and the mutation tests should cover those cases.
 */

waitForExplicitFinish();

let doc, inspector, markup;

let TEST_URL = "http://mochi.test:8888/browser/browser/devtools/markupview/test/browser_inspector_markup_edit.html";
let LONG_ATTRIBUTE = "ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ";
let LONG_ATTRIBUTE_COLLAPSED = "ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEF\u2026UVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ";
let DATA_URL_INLINE_STYLE='color: red; background: url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQAQMAAAAlPW0iAAAABlBMVEUAAAD///+l2Z/dAAAAM0lEQVR4nGP4/5/h/1+G/58ZDrAz3D/McH8yw83NDDeNGe4Ug9C9zwz3gVLMDA/A6P9/AFGGFyjOXZtQAAAAAElFTkSuQmCC");';
let DATA_URL_INLINE_STYLE_COLLAPSED='color: red; background: url("data:image/png;base64,iVBORw0KG\u2026NDDeNGe4Ug9C9zwz3gVLMDA/A6P9/AFGGFyjOXZtQAAAAAElFTkSuQmCC");';
let DATA_URL_ATTRIBUTE = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQAQMAAAAlPW0iAAAABlBMVEUAAAD///+l2Z/dAAAAM0lEQVR4nGP4/5/h/1+G/58ZDrAz3D/McH8yw83NDDeNGe4Ug9C9zwz3gVLMDA/A6P9/AFGGFyjOXZtQAAAAAElFTkSuQmCC";
let DATA_URL_ATTRIBUTE_COLLAPSED = "data:image/png;base64,iVBORw0K\u20269/AFGGFyjOXZtQAAAAAElFTkSuQmCC";

let TEST_DATA = [
  {
    desc: "Change an attribute",
    before: function() {
      assertAttributes(doc.querySelector("#node1"), {
        id: "node1",
        class: "node1"
      });
    },
    execute: function(after) {
      inspector.once("markupmutation", after);
      let editor = getContainerForRawNode(markup, doc.querySelector("#node1")).editor;
      let attr = editor.attrs["class"].querySelector(".editable");
      setEditableFieldValue(attr, 'class="changednode1"', inspector);
    },
    after: function() {
      assertAttributes(doc.querySelector("#node1"), {
        id: "node1",
        class: "changednode1"
      });
    }
  },
  {
    desc: 'Try changing an attribute to a quote (") - this should result ' +
          'in it being set to an empty string',
    before: function() {
      assertAttributes(doc.querySelector("#node22"), {
        id: "node22",
        class: "unchanged"
      });
    },
    execute: function(after) {
      let editor = getContainerForRawNode(markup, doc.querySelector("#node22")).editor;
      let attr = editor.attrs["class"].querySelector(".editable");
      setEditableFieldValue(attr, 'class="""', inspector);
      inspector.once("markupmutation", after);
    },
    after: function() {
      assertAttributes(doc.querySelector("#node22"), {
        id: "node22",
        class: ""
      });
    }
  },
  {
    desc: "Remove an attribute",
    before: function() {
      assertAttributes(doc.querySelector("#node4"), {
        id: "node4",
        class: "node4"
      });
    },
    execute: function(after) {
      inspector.once("markupmutation", after);
      let editor = getContainerForRawNode(markup, doc.querySelector("#node4")).editor;
      let attr = editor.attrs["class"].querySelector(".editable");
      setEditableFieldValue(attr, '', inspector);
    },
    after: function() {
      assertAttributes(doc.querySelector("#node4"), {
        id: "node4",
      });
    }
  },
  {
    desc: "Add an attribute by clicking the empty space after a node",
    before: function() {
      assertAttributes(doc.querySelector("#node14"), {
        id: "node14",
      });
    },
    execute: function(after) {
      inspector.once("markupmutation", after);
      let editor = getContainerForRawNode(markup, doc.querySelector("#node14")).editor;
      let attr = editor.newAttr;
      setEditableFieldValue(attr, 'class="newclass" style="color:green"', inspector);
    },
    after: function() {
      assertAttributes(doc.querySelector("#node14"), {
        id: "node14",
        class: "newclass",
        style: "color:green"
      });
    }
  },
  {
    desc: 'Try add an attribute containing a quote (") attribute by ' +
          'clicking the empty space after a node - this should result ' +
          'in it being set to an empty string',
    before: function() {
      assertAttributes(doc.querySelector("#node23"), {
        id: "node23",
      });
    },
    execute: function(after) {
      let editor = getContainerForRawNode(markup, doc.querySelector("#node23")).editor;
      let attr = editor.newAttr;
      setEditableFieldValue(attr, 'class="newclass" style="""', inspector);
      inspector.once("markupmutation", after);
    },
    after: function() {
      assertAttributes(doc.querySelector("#node23"), {
        id: "node23",
        class: "newclass",
        style: ""
      });
    }
  },
  {
    desc: "Try add attributes by adding to an existing attribute's entry",
    before: function() {
      assertAttributes(doc.querySelector("#node24"), {
        id: "node24",
      });
    },
    execute: function(after) {
      let editor = getContainerForRawNode(markup, doc.querySelector("#node24")).editor;
      let attr = editor.attrs["id"].querySelector(".editable");
      setEditableFieldValue(attr, attr.textContent + ' class="""', inspector);
      inspector.once("markupmutation", after);
    },
    after: function() {
      assertAttributes(doc.querySelector("#node24"), {
        id: "node24",
        class: ""
      });
    }
  },
  {
    desc: "Try to add long attribute to make sure it is collapsed in attribute editor.",
    before: function() {
      assertAttributes(doc.querySelector("#node24"), {
        id: "node24",
        class: ""
      });
    },
    execute: function(after) {
      let editor = getContainerForRawNode(markup, doc.querySelector("#node24")).editor;
      let attr = editor.newAttr;
      setEditableFieldValue(attr, 'data-long="'+LONG_ATTRIBUTE+'"', inspector);
      inspector.once("markupmutation", after);
    },
    after: function() {

      let editor = getContainerForRawNode(markup, doc.querySelector("#node24")).editor;
      let visibleAttrText = editor.attrs["data-long"].querySelector(".attr-value").textContent;
      is (visibleAttrText, LONG_ATTRIBUTE_COLLAPSED)

      assertAttributes(doc.querySelector("#node24"), {
        id: "node24",
        class: "",
        'data-long':LONG_ATTRIBUTE
      });
    }
  },
  {
    desc: "Try to modify the collapsed long attribute, making sure it expands.",
    before: function() {
      assertAttributes(doc.querySelector("#node24"), {
        id: "node24",
        class: "",
        'data-long': LONG_ATTRIBUTE
      });
    },
    execute: function(after) {
      let editor = getContainerForRawNode(markup, doc.querySelector("#node24")).editor;
      let attr = editor.attrs["data-long"].querySelector(".editable");

      // Check to make sure it has expanded after focus
      attr.focus();
      EventUtils.sendKey("return", inspector.panelWin);
      let input = inplaceEditor(attr).input;
      is (input.value, 'data-long="'+LONG_ATTRIBUTE+'"');
      EventUtils.sendKey("escape", inspector.panelWin);

      setEditableFieldValue(attr, input.value  + ' data-short="ABC"', inspector);
      inspector.once("markupmutation", after);
    },
    after: function() {

      let editor = getContainerForRawNode(markup, doc.querySelector("#node24")).editor;
      let visibleAttrText = editor.attrs["data-long"].querySelector(".attr-value").textContent;
      is (visibleAttrText, LONG_ATTRIBUTE_COLLAPSED)

      assertAttributes(doc.querySelector("#node24"), {
        id: "node24",
        class: "",
        'data-long': LONG_ATTRIBUTE,
        "data-short": "ABC"
      });
    }
  },
  {
    desc: "Try to add long data URL to make sure it is collapsed in attribute editor.",
    before: function() {
      assertAttributes(doc.querySelector("#node-data-url"), {
        id: "node-data-url"
      });
    },
    execute: function(after) {
      let editor = getContainerForRawNode(markup, doc.querySelector("#node-data-url")).editor;
      let attr = editor.newAttr;
      setEditableFieldValue(attr, 'src="'+DATA_URL_ATTRIBUTE+'"', inspector);
      inspector.once("markupmutation", after);
    },
    after: function() {

      let editor = getContainerForRawNode(markup, doc.querySelector("#node-data-url")).editor;
      let visibleAttrText = editor.attrs["src"].querySelector(".attr-value").textContent;
      is (visibleAttrText, DATA_URL_ATTRIBUTE_COLLAPSED);

      let node = doc.querySelector("#node-data-url");
      is (node.width, 16, "Image width has been set after data url src.");
      is (node.height, 16, "Image height has been set after data url src.");

      assertAttributes(node, {
        id: "node-data-url",
        "src": DATA_URL_ATTRIBUTE
      });
    }
  },
  {
    desc: "Try to add long data URL to make sure it is collapsed in attribute editor.",
    before: function() {
      assertAttributes(doc.querySelector("#node-data-url-style"), {
        id: "node-data-url-style"
      });
    },
    execute: function(after) {
      let editor = getContainerForRawNode(markup, doc.querySelector("#node-data-url-style")).editor;
      let attr = editor.newAttr;
      setEditableFieldValue(attr, "style='"+DATA_URL_INLINE_STYLE+"'", inspector);
      inspector.once("markupmutation", after);
    },
    after: function() {

      let editor = getContainerForRawNode(markup, doc.querySelector("#node-data-url-style")).editor;
      let visibleAttrText = editor.attrs["style"].querySelector(".attr-value").textContent;
      is (visibleAttrText, DATA_URL_INLINE_STYLE_COLLAPSED)

      assertAttributes(doc.querySelector("#node-data-url-style"), {
        id: "node-data-url-style",
        'style': DATA_URL_INLINE_STYLE
      });
    }
  },
  {
    desc: "Edit text",
    before: function() {
      let node = doc.querySelector('.node6').firstChild;
      is(node.nodeValue, "line6", "Text should be unchanged");
    },
    execute: function(after) {
      inspector.once("markupmutation", after);
      let node = doc.querySelector('.node6').firstChild;
      let editor = getContainerForRawNode(markup, node).editor;
      let field = editor.elt.querySelector("pre");
      setEditableFieldValue(field, "New text", inspector);
    },
    after: function() {
      let node = doc.querySelector('.node6').firstChild;
      is(node.nodeValue, "New text", "Text should be changed.");
    },
  },
  {
    desc: "Add an attribute value containing < > &uuml; \" & '",
    before: function() {
      assertAttributes(doc.querySelector("#node25"), {
        id: "node25",
      });
    },
    execute: function(after) {
      inspector.once("markupmutation", after);
      let editor = getContainerForRawNode(markup, doc.querySelector("#node25")).editor;
      let attr = editor.newAttr;
      setEditableFieldValue(attr, 'src="somefile.html?param1=<a>&param2=&uuml;&param3=\'&quot;\'"', inspector);
    },
    after: function() {
      assertAttributes(doc.querySelector("#node25"), {
        id: "node25",
        src: "somefile.html?param1=<a>&param2=\xfc&param3='\"'"
      });
    }
  },
  {
    desc: "Modify inline style containing \"",
    before: function() {
      assertAttributes(doc.querySelector("#node26"), {
        id: "node26",
        style: 'background-image: url("moz-page-thumb://thumbnail?url=http%3A%2F%2Fwww.mozilla.org%2F");'
      });
    },
    execute: function(after) {
      inspector.once("markupmutation", after);
      let editor = getContainerForRawNode(markup, doc.querySelector("#node26")).editor;
      let attr = editor.attrs["style"].querySelector(".editable");


      attr.focus();
      EventUtils.sendKey("return", inspector.panelWin);

      let input = inplaceEditor(attr).input;
      let value = input.value;

      is (value,
        "style='background-image: url(\"moz-page-thumb://thumbnail?url=http%3A%2F%2Fwww.mozilla.org%2F\");'",
        "Value contains actual double quotes"
      );

      value = value.replace(/mozilla\.org/, "mozilla.com");
      input.value = value;

      EventUtils.sendKey("return", inspector.panelWin);
    },
    after: function() {
      assertAttributes(doc.querySelector("#node26"), {
        id: "node26",
        style: 'background-image: url("moz-page-thumb://thumbnail?url=http%3A%2F%2Fwww.mozilla.com%2F");'
      });
    }
  },
  {
    desc: "Modify inline style containing \" and \'",
    before: function() {
      assertAttributes(doc.querySelector("#node27"), {
        id: "node27",
        class: 'Double " and single \''
      });
    },
    execute: function(after) {
      inspector.once("markupmutation", after);
      let editor = getContainerForRawNode(markup, doc.querySelector("#node27")).editor;
      let attr = editor.attrs["class"].querySelector(".editable");

      attr.focus();
      EventUtils.sendKey("return", inspector.panelWin);

      let input = inplaceEditor(attr).input;
      let value = input.value;

      is (value, "class=\"Double &quot; and single '\"", "Value contains &quot;");

      value = value.replace(/Double/, "&quot;").replace(/single/, "'");
      input.value = value;

      EventUtils.sendKey("return", inspector.panelWin);
    },
    after: function() {
      assertAttributes(doc.querySelector("#node27"), {
        id: "node27",
        class: '" " and \' \''
      });
    }
  }
];

function test() {
  addTab(TEST_URL).then(openInspector).then(args => {
    inspector = args.inspector;
    doc = content.document;
    markup = inspector.markup;

    markup.expandAll().then(() => {
      // Iterate through the items in TEST_DATA
      let cursor = 0;

      function nextEditTest() {
        executeSoon(function() {
          if (cursor >= TEST_DATA.length) {
            return finishUp();
          }

          let step = TEST_DATA[cursor++];
          info("Start test for: " + step.desc);
          if (step.setup) {
            step.setup();
          }
          step.before();
          step.execute(function() {
            step.after();

            undoChange(inspector).then(() => {
              step.before();

              redoChange(inspector).then(() => {
                step.after();
                info("End test for: " + step.desc);
                nextEditTest();
              });
            });
          });
        });
      }

      nextEditTest();
    });
  });
}

function finishUp() {
  while (markup.undo.canUndo()) {
    markup.undo.undo();
  }
  inspector.once("inspector-updated", () => {
    doc = inspector = markup = null;
    gBrowser.removeCurrentTab();
    finish();
  });
}
