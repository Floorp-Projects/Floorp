/* Any copyright", " is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that various editors work as expected.  Also checks
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

function test() {
  let inspector;
  let {
    getInplaceEditorForSpan: inplaceEditor
  } = devtools.require("devtools/shared/inplace-editor");

  // Prevent intermittent "test exceeded the timeout threshold" since this is
  // a slow test: https://bugzilla.mozilla.org/show_bug.cgi?id=904953.
  requestLongerTimeout(2);

  waitForExplicitFinish();

  // Will hold the doc we're viewing
  let doc;

  // Holds the MarkupTool object we're testing.
  let markup;

  let LONG_ATTRIBUTE = "ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  let LONG_ATTRIBUTE_COLLAPSED = "ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEF\u2026UVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ";

  let DATA_URL_INLINE_STYLE='color: red; background: url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQAQMAAAAlPW0iAAAABlBMVEUAAAD///+l2Z/dAAAAM0lEQVR4nGP4/5/h/1+G/58ZDrAz3D/McH8yw83NDDeNGe4Ug9C9zwz3gVLMDA/A6P9/AFGGFyjOXZtQAAAAAElFTkSuQmCC");';
  let DATA_URL_INLINE_STYLE_COLLAPSED='color: red; background: url("data:image/png;base64,iVBORw0KG\u2026NDDeNGe4Ug9C9zwz3gVLMDA/A6P9/AFGGFyjOXZtQAAAAAElFTkSuQmCC");';

  let DATA_URL_ATTRIBUTE = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQAQMAAAAlPW0iAAAABlBMVEUAAAD///+l2Z/dAAAAM0lEQVR4nGP4/5/h/1+G/58ZDrAz3D/McH8yw83NDDeNGe4Ug9C9zwz3gVLMDA/A6P9/AFGGFyjOXZtQAAAAAElFTkSuQmCC";
  let DATA_URL_ATTRIBUTE_COLLAPSED = "data:image/png;base64,iVBORw0K\u20269/AFGGFyjOXZtQAAAAAElFTkSuQmCC";

  /**
   * Edit a given editableField
   */
  function editField(aField, aValue)
  {
    aField.focus();
    EventUtils.sendKey("return", inspector.panelWin);
    let input = inplaceEditor(aField).input;
    input.value = aValue;
    EventUtils.sendKey("return", inspector.panelWin);
  }

  /**
   * Check that the appropriate attributes are assigned to a node.
   *
   * @param  {HTMLNode} aElement
   *         The node to check.
   * @param  {Object} aAttributes
   *         An object containing the arguments to check.
   *         e.g. {id="id1",class="someclass"}
   *
   * NOTE: When checking attribute values bare in mind that node.getAttribute()
   *       returns attribute values provided by the HTML parser. The parser only
   *       provides unescaped entities so &amp; will return &.
   */
  function assertAttributes(aElement, aAttributes)
  {
    let attrs = Object.getOwnPropertyNames(aAttributes);
    is(aElement.attributes.length, attrs.length,
      "Node has the correct number of attributes");
    for (let attr of attrs) {
      is(aElement.getAttribute(attr), aAttributes[attr],
        "Node has the correct " + attr + " attribute.");
    }
  }

  // All the mutation types we want to test.
  let edits = [
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
        editField(attr, 'class="changednode1"');
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
        editField(attr, 'class="""');
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
        editField(attr, '');
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
        editField(attr, 'class="newclass" style="color:green"');
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
        editField(attr, 'class="newclass" style="""');
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
        editField(attr, attr.textContent + ' class="""');
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
        editField(attr, 'data-long="'+LONG_ATTRIBUTE+'"');
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

        editField(attr, input.value  + ' data-short="ABC"');
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
        editField(attr, 'src="'+DATA_URL_ATTRIBUTE+'"');
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
        editField(attr, "style='"+DATA_URL_INLINE_STYLE+"'");
        inspector.once("markupmutation", after);
      },
      after: function() {

        let editor = getContainerForRawNode(markup, doc.querySelector("#node-data-url-style")).editor;
        let visibleAttrText = editor.attrs["style"].querySelector(".attr-value").textContent;
        is (visibleAttrText, DATA_URL_INLINE_STYLE_COLLAPSED)

        assertAttributes(doc.querySelector("#node-data-url-style"), {
          id: "node-data-url-style",
          'style':DATA_URL_INLINE_STYLE
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
        editField(field, "New text");
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
        editField(attr, 'src="somefile.html?param1=<a>&param2=&uuml;&param3=\'&quot;\'"');
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
    },

    {
      desc: "Add an attribute value without closing \"",
      enteredText: 'style="display: block;',
      expectedAttributes: {
        style: "display: block;"
      }
    },
    {
      desc: "Add an attribute value without closing '",
      enteredText: "style='display: inline;",
      expectedAttributes: {
        style: "display: inline;"
      }
    },
    {
      desc: "Add an attribute wrapped with with double quotes double quote in it",
      enteredText: 'style="display: "inline',
      expectedAttributes: {
        style: "display: ",
        inline: ""
      }
    },
    {
      desc: "Add an attribute wrapped with single quotes with single quote in it",
      enteredText: "style='display: 'inline",
      expectedAttributes: {
        style: "display: ",
        inline: ""
      }
    },
    {
      desc: "Add an attribute with no value",
      enteredText: "disabled",
      expectedAttributes: {
        disabled: ""
      }
    },
    {
      desc: "Add multiple attributes with no value",
      enteredText: "disabled autofocus",
      expectedAttributes: {
        disabled: "",
        autofocus: ""
      }
    },
    {
      desc: "Add multiple attributes with no value, and some with value",
      enteredText: "disabled name='name' data-test='test' autofocus",
      expectedAttributes: {
        disabled: "",
        autofocus: "",
        name: "name",
        'data-test': "test"
      }
    },
    {
      desc: "Add attribute with xmlns",
      enteredText: "xmlns:edi='http://ecommerce.example.org/schema'",
      expectedAttributes: {
        'xmlns:edi': "http://ecommerce.example.org/schema"
      }
    },
    {
      desc: "Mixed single and double quotes",
      enteredText: "name=\"hi\" maxlength='not a number'",
      expectedAttributes: {
        maxlength: "not a number",
        name: "hi"
      }
    },
    {
      desc: "Invalid attribute name",
      enteredText: "x='y' <why-would-you-do-this>=\"???\"",
      expectedAttributes: {
        x: "y"
      }
    },
    {
      desc: "Double quote wrapped in single quotes",
      enteredText: "x='h\"i'",
      expectedAttributes: {
        x: "h\"i"
      }
    },
    {
      desc: "Single quote wrapped in double quotes",
      enteredText: "x=\"h'i\"",
      expectedAttributes: {
        x: "h'i"
      }
    },
    {
      desc: "No quote wrapping",
      enteredText: "a=b x=y data-test=Some spaced data",
      expectedAttributes: {
        a: "b",
        x: "y",
        "data-test": "Some",
        spaced: "",
        data: ""
      }
    },
    {
      desc: "Duplicate Attributes",
      enteredText: "a=b a='c' a=\"d\"",
      expectedAttributes: {
        a: "b"
      }
    },
    {
      desc: "Inline styles",
      enteredText: "style=\"font-family: 'Lucida Grande', sans-serif; font-size: 75%;\"",
      expectedAttributes: {
        style: "font-family: 'Lucida Grande', sans-serif; font-size: 75%;"
      }
    },
    {
      desc: "Object attribute names",
      enteredText: "toString=\"true\" hasOwnProperty=\"false\"",
      expectedAttributes: {
        toString: "true",
        hasOwnProperty: "false"
      }
    },
    {
      desc: "Add event handlers",
      enteredText: "onclick=\"javascript: throw new Error('wont fire');\" onload=\"alert('here');\"",
      expectedAttributes: {
        onclick: "javascript: throw new Error('wont fire');",
        onload: "alert('here');"
      }
    }
  ];

  // Create the helper tab for parsing...
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(setupTest, content);
  }, true);
  content.location = "http://mochi.test:8888/browser/browser/devtools/markupview/test/browser_inspector_markup_edit.html";

  function setupTest() {
    var target = TargetFactory.forTab(gBrowser.selectedTab);
    gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
      inspector = toolbox.getCurrentPanel();
      startTests();
    });
  }

  function startTests() {
    markup = inspector.markup;

    // expectedAttributes - Shortcut to provide a more decalarative test when you only
    // want to check the outcome of setting an attribute to a string.
    edits.forEach((edit, i) => {
      if (edit.expectedAttributes) {
        let id = "expectedAttributes" + i;

        let div = doc.createElement("div");
        div.id = id;
        doc.body.appendChild(div);

        // Attach the ID onto the object that will assert attributes
        edit.expectedAttributes.id = id;

        edit.before = () => {
          assertAttributes(doc.querySelector("#" + id), {
            id: id,
          });
        };

        edit.execute = (after) =>{
          inspector.once("markupmutation", after);
          let editor = getContainerForRawNode(markup, doc.querySelector("#" + id)).editor;
          editField(editor.newAttr, edit.enteredText);
        };

        edit.after = () => {
          assertAttributes(doc.querySelector("#" + id), edit.expectedAttributes);
        };
      }
    });

    markup.expandAll().then(() => {

      let cursor = 0;

      function nextEditTest() {
        executeSoon(function() {
          if (cursor >= edits.length) {
            addAttributes();
          } else {
            let step = edits[cursor++];
            info("START " + step.desc);
            if (step.setup) {
              step.setup();
            }
            step.before();
            info("before execute");
            step.execute(function() {
              info("after execute");
              step.after();
              ok(markup.undo.canUndo(), "Should be able to undo.");
              markup.undo.undo();
              inspector.once("markupmutation", () => {
                step.before();
                ok(markup.undo.canRedo(), "Should be able to redo.");
                markup.undo.redo();
                inspector.once("markupmutation", () => {
                  step.after();
                  info("END " + step.desc);
                  nextEditTest();
                });
              });
            });
          }
        });
      }
      nextEditTest();
    });
  }

  function addAttributes() {
    let test = {
      desc: "Add attributes by adding to an existing attribute's entry",
      setup: function() {
        inspector.selection.setNode(doc.querySelector("#node18"));
      },
      before: function() {
        assertAttributes(doc.querySelector("#node18"), {
          id: "node18",
        });

        is(inspector.highlighter.nodeInfo.classesBox.textContent, "",
           "No classes in the infobar before edit.");
      },
      execute: function(after) {
        inspector.once("markupmutation", function() {
          // needed because we need to make sure the infobar is updated
          // not just the markupview (which happens in this event loop)
          executeSoon(after);
        });
        let editor = getContainerForRawNode(markup, doc.querySelector("#node18")).editor;
        let attr = editor.attrs["id"].querySelector(".editable");
        editField(attr, attr.textContent + ' class="newclass" style="color:green"');
      },
      after: function() {
        assertAttributes(doc.querySelector("#node18"), {
          id: "node18",
          class: "newclass",
          style: "color:green"
        });

        is(inspector.highlighter.nodeInfo.classesBox.textContent, ".newclass",
           "Correct classes in the infobar after edit.");
      }
    };
    testAsyncSetup(test, editTagName);
  }

  function editTagName() {
    let test =  {
      desc: "Edit the tag name",
      setup: function() {
        inspector.selection.setNode(doc.querySelector("#retag-me"));
      },
      before: function() {
        let node = doc.querySelector("#retag-me");
        let container = getContainerForRawNode(markup, node);

        is(node.tagName, "DIV", "retag-me should be a div.");
        ok(container.selected, "retag-me should be selected.");
        ok(container.expanded, "retag-me should be expanded.");
        is(doc.querySelector("#retag-me-2").parentNode, node,
          "retag-me-2 should be a child of the old element.");
      },
      execute: function(after) {
        inspector.once("markupmutation", after);
        let node = doc.querySelector("#retag-me");
        let editor = getContainerForRawNode(markup, node).editor;
        let field = editor.tag;
        editField(field, "p");
      },
      after: function() {
        let node = doc.querySelector("#retag-me");
        let container = getContainerForRawNode(markup, node);
        is(node.tagName, "P", "retag-me should be a p.");
        ok(container.selected, "retag-me should be selected.");
        ok(container.expanded, "retag-me should be expanded.");
        is(doc.querySelector("#retag-me-2").parentNode, node,
          "retag-me-2 should be a child of the new element.");
      }
    };
    testAsyncSetup(test, removeElementWithDelete);
  }

  function removeElementWithDelete() {
    let test =  {
      desc: "Remove an element with the delete key",
      before: function() {
        ok(!!doc.querySelector("#node18"), "Node 18 should exist.");
      },
      execute: function() {
        inspector.selection.setNode(doc.querySelector("#node18"));
      },
      executeCont: function() {
        EventUtils.sendKey("delete", inspector.panelWin);
      },
      after: function() {
        ok(!doc.querySelector("#node18"), "Node 18 should not exist.")
      }
    };
    testAsyncExecute(test, finishUp);
  }

  function testAsyncExecute(test, callback) {
    info("START " + test.desc);

    test.before();
    inspector.once("inspector-updated", function BIMET_testAsyncExecNewNode() {
      test.executeCont();
      inspector.once("markupmutation", () => {
        test.after();
        undoRedo(test, callback);
      });
    });
    executeSoon(function BIMET_setNode1() {
      test.execute();
    });
  }

  function testAsyncSetup(test, callback) {
    info("START " + test.desc);

    inspector.on("inspector-updated", function BIMET_updated(event, name) {
      if (name === "inspector-panel") {
        inspector.off("inspector-updated", BIMET_updated);

        test.before();
        test.execute(function() {
          test.after();
          undoRedo(test, callback);
        });
      }
    });
    executeSoon(test.setup);
  }

  function undoRedo(test, callback) {
    ok(markup.undo.canUndo(), "Should be able to undo.");
    markup.undo.undo();
    inspector.once("markupmutation", () => {
      test.before();
      ok(markup.undo.canRedo(), "Should be able to redo.");
      markup.undo.redo();
      inspector.once("markupmutation", () => {
        test.after();
        info("END " + test.desc);
        callback();
      });
    });
  }

  function finishUp() {
    while (markup.undo.canUndo()) {
      markup.undo.undo();
    }
    doc = inspector = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
