/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that various editors work as expected. Also checks
// that the various changes are properly undoable and redoable.
// For each step in the test, we:
// - Check that the node we're editing is as we expect
// - Make the change, check that the change was made as we expect
// - Undo the change, check that the node is back in its original state
// - Redo the change, check that the node change was made again correctly.
// This test mostly tries to verify that the editor makes changes to the
// underlying DOM, not that the UI updates - UI updates are based on
// underlying DOM changes, and the mutation tests should cover those cases.

let TEST_URL = TEST_URL_ROOT + "doc_markup_edit.html";
let LONG_ATTRIBUTE = "ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ";
let LONG_ATTRIBUTE_COLLAPSED = "ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEF\u2026UVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ";
let DATA_URL_INLINE_STYLE='color: red; background: url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQAQMAAAAlPW0iAAAABlBMVEUAAAD///+l2Z/dAAAAM0lEQVR4nGP4/5/h/1+G/58ZDrAz3D/McH8yw83NDDeNGe4Ug9C9zwz3gVLMDA/A6P9/AFGGFyjOXZtQAAAAAElFTkSuQmCC");';
let DATA_URL_INLINE_STYLE_COLLAPSED='color: red; background: url("data:image/png;base64,iVBORw0KG\u2026NDDeNGe4Ug9C9zwz3gVLMDA/A6P9/AFGGFyjOXZtQAAAAAElFTkSuQmCC");';
let DATA_URL_ATTRIBUTE = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQAQMAAAAlPW0iAAAABlBMVEUAAAD///+l2Z/dAAAAM0lEQVR4nGP4/5/h/1+G/58ZDrAz3D/McH8yw83NDDeNGe4Ug9C9zwz3gVLMDA/A6P9/AFGGFyjOXZtQAAAAAElFTkSuQmCC";
let DATA_URL_ATTRIBUTE_COLLAPSED = "data:image/png;base64,iVBORw0K\u20269/AFGGFyjOXZtQAAAAAElFTkSuQmCC";

let TEST_DATA = [
  {
    desc: "Change an attribute",
    before: function(inspector) {
      assertAttributes("#node1", {
        id: "node1",
        class: "node1"
      });
    },
    execute: function(after, inspector) {
      inspector.once("markupmutation", after);
      let editor = getContainerForRawNode("#node1", inspector).editor;
      let attr = editor.attrs["class"].querySelector(".editable");
      setEditableFieldValue(attr, 'class="changednode1"', inspector);
    },
    after: function(inspector) {
      assertAttributes("#node1", {
        id: "node1",
        class: "changednode1"
      });
    }
  },
  {
    desc: 'Try changing an attribute to a quote (") - this should result ' +
          'in it being set to an empty string',
    before: function(inspector) {
      assertAttributes("#node22", {
        id: "node22",
        class: "unchanged"
      });
    },
    execute: function(after, inspector) {
      let editor = getContainerForRawNode("#node22", inspector).editor;
      let attr = editor.attrs["class"].querySelector(".editable");
      setEditableFieldValue(attr, 'class="""', inspector);
      inspector.once("markupmutation", after);
    },
    after: function(inspector) {
      assertAttributes("#node22", {
        id: "node22",
        class: ""
      });
    }
  },
  {
    desc: "Remove an attribute",
    before: function(inspector) {
      assertAttributes("#node4", {
        id: "node4",
        class: "node4"
      });
    },
    execute: function(after, inspector) {
      inspector.once("markupmutation", after);
      let editor = getContainerForRawNode("#node4", inspector).editor;
      let attr = editor.attrs["class"].querySelector(".editable");
      setEditableFieldValue(attr, '', inspector);
    },
    after: function(inspector) {
      assertAttributes("#node4", {
        id: "node4",
      });
    }
  },
  {
    desc: "Add an attribute by clicking the empty space after a node",
    before: function(inspector) {
      assertAttributes("#node14", {
        id: "node14",
      });
    },
    execute: function(after, inspector) {
      inspector.once("markupmutation", after);
      let editor = getContainerForRawNode("#node14", inspector).editor;
      let attr = editor.newAttr;
      setEditableFieldValue(attr, 'class="newclass" style="color:green"', inspector);
    },
    after: function(inspector) {
      assertAttributes("#node14", {
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
    before: function(inspector) {
      assertAttributes("#node23", {
        id: "node23",
      });
    },
    execute: function(after, inspector) {
      let editor = getContainerForRawNode("#node23", inspector).editor;
      let attr = editor.newAttr;
      setEditableFieldValue(attr, 'class="newclass" style="""', inspector);
      inspector.once("markupmutation", after);
    },
    after: function(inspector) {
      assertAttributes("#node23", {
        id: "node23",
        class: "newclass",
        style: ""
      });
    }
  },
  {
    desc: "Try add attributes by adding to an existing attribute's entry",
    before: function(inspector) {
      assertAttributes("#node24", {
        id: "node24",
      });
    },
    execute: function(after, inspector) {
      let editor = getContainerForRawNode("#node24", inspector).editor;
      let attr = editor.attrs["id"].querySelector(".editable");
      setEditableFieldValue(attr, attr.textContent + ' class="""', inspector);
      inspector.once("markupmutation", after);
    },
    after: function(inspector) {
      assertAttributes("#node24", {
        id: "node24",
        class: ""
      });
    }
  },
  {
    desc: "Try to add long attribute to make sure it is collapsed in attribute editor.",
    before: function(inspector) {
      assertAttributes("#node24", {
        id: "node24",
        class: ""
      });
    },
    execute: function(after, inspector) {
      let editor = getContainerForRawNode("#node24", inspector).editor;
      let attr = editor.newAttr;
      setEditableFieldValue(attr, 'data-long="'+LONG_ATTRIBUTE+'"', inspector);
      inspector.once("markupmutation", after);
    },
    after: function(inspector) {
      let editor = getContainerForRawNode("#node24", inspector).editor;
      let visibleAttrText = editor.attrs["data-long"].querySelector(".attr-value").textContent;
      is (visibleAttrText, LONG_ATTRIBUTE_COLLAPSED)

      assertAttributes("#node24", {
        id: "node24",
        class: "",
        'data-long':LONG_ATTRIBUTE
      });
    }
  },
  {
    desc: "Try to modify the collapsed long attribute, making sure it expands.",
    before: function(inspector) {
      assertAttributes("#node24", {
        id: "node24",
        class: "",
        'data-long': LONG_ATTRIBUTE
      });
    },
    execute: function(after, inspector) {
      let editor = getContainerForRawNode("#node24", inspector).editor;
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
    after: function(inspector) {
      let editor = getContainerForRawNode("#node24", inspector).editor;
      let visibleAttrText = editor.attrs["data-long"].querySelector(".attr-value").textContent;
      is (visibleAttrText, LONG_ATTRIBUTE_COLLAPSED)

      assertAttributes("#node24", {
        id: "node24",
        class: "",
        'data-long': LONG_ATTRIBUTE,
        "data-short": "ABC"
      });
    }
  },
  {
    desc: "Try to add long data URL to make sure it is collapsed in attribute editor.",
    before: function(inspector) {
      assertAttributes("#node-data-url", {
        id: "node-data-url"
      });
    },
    execute: function(after, inspector) {
      let editor = getContainerForRawNode("#node-data-url", inspector).editor;
      let attr = editor.newAttr;
      setEditableFieldValue(attr, 'src="'+DATA_URL_ATTRIBUTE+'"', inspector);
      inspector.once("markupmutation", after);
    },
    after: function(inspector) {
      let editor = getContainerForRawNode("#node-data-url", inspector).editor;
      let visibleAttrText = editor.attrs["src"].querySelector(".attr-value").textContent;
      is (visibleAttrText, DATA_URL_ATTRIBUTE_COLLAPSED);

      let node = getNode("#node-data-url");
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
    before: function(inspector) {
      assertAttributes("#node-data-url-style", {
        id: "node-data-url-style"
      });
    },
    execute: function(after, inspector) {
      let editor = getContainerForRawNode("#node-data-url-style", inspector).editor;
      let attr = editor.newAttr;
      setEditableFieldValue(attr, "style='"+DATA_URL_INLINE_STYLE+"'", inspector);
      inspector.once("markupmutation", after);
    },
    after: function(inspector) {
      let editor = getContainerForRawNode("#node-data-url-style", inspector).editor;
      let visibleAttrText = editor.attrs["style"].querySelector(".attr-value").textContent;
      is (visibleAttrText, DATA_URL_INLINE_STYLE_COLLAPSED)

      assertAttributes("#node-data-url-style", {
        id: "node-data-url-style",
        'style': DATA_URL_INLINE_STYLE
      });
    }
  },
  {
    desc: "Edit text",
    before: function(inspector) {
      let node = getNode('.node6').firstChild;
      is(node.nodeValue, "line6", "Text should be unchanged");
    },
    execute: function(after, inspector) {
      inspector.once("markupmutation", after);
      let node = getNode('.node6').firstChild;
      let editor = getContainerForRawNode(node, inspector).editor;
      let field = editor.elt.querySelector("pre");
      setEditableFieldValue(field, "New text", inspector);
    },
    after: function(inspector) {
      let node = getNode('.node6').firstChild;
      is(node.nodeValue, "New text", "Text should be changed.");
    },
  },
  {
    desc: "Add an attribute value containing < > &uuml; \" & '",
    before: function(inspector) {
      assertAttributes("#node25", {
        id: "node25",
      });
    },
    execute: function(after, inspector) {
      inspector.once("markupmutation", after);
      let editor = getContainerForRawNode("#node25", inspector).editor;
      let attr = editor.newAttr;
      setEditableFieldValue(attr, 'src="somefile.html?param1=<a>&param2=&uuml;&param3=\'&quot;\'"', inspector);
    },
    after: function(inspector) {
      assertAttributes("#node25", {
        id: "node25",
        src: "somefile.html?param1=<a>&param2=\xfc&param3='\"'"
      });
    }
  },
  {
    desc: "Modify inline style containing \"",
    before: function(inspector) {
      assertAttributes("#node26", {
        id: "node26",
        style: 'background-image: url("moz-page-thumb://thumbnail?url=http%3A%2F%2Fwww.mozilla.org%2F");'
      });
    },
    execute: function(after, inspector) {
      inspector.once("markupmutation", after);
      let editor = getContainerForRawNode("#node26", inspector).editor;
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
    after: function(inspector) {
      assertAttributes("#node26", {
        id: "node26",
        style: 'background-image: url("moz-page-thumb://thumbnail?url=http%3A%2F%2Fwww.mozilla.com%2F");'
      });
    }
  },
  {
    desc: "Modify inline style containing \" and \'",
    before: function(inspector) {
      assertAttributes("#node27", {
        id: "node27",
        class: 'Double " and single \''
      });
    },
    execute: function(after, inspector) {
      inspector.once("markupmutation", after);
      let editor = getContainerForRawNode("#node27", inspector).editor;
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
    after: function(inspector) {
      assertAttributes("#node27", {
        id: "node27",
        class: '" " and \' \''
      });
    }
  }
];

let test = asyncTest(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  info("Expanding all nodes in the markup-view");
  yield inspector.markup.expandAll();

  for (let step of TEST_DATA) {
    yield executeStep(step, inspector);
  }
  yield inspector.once("inspector-updated");
});

function executeStep(step, inspector) {
  let def = promise.defer();

  info("Start test for: " + step.desc);
  step.before(inspector);
  step.execute(function() {
    step.after(inspector);

    undoChange(inspector).then(() => {
      step.before(inspector);

      redoChange(inspector).then(() => {
        step.after(inspector);
        info("End test for: " + step.desc);
        def.resolve();
      });
    });
  }, inspector);

  return def.promise;
}
