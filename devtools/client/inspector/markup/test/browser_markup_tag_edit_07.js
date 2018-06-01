/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_attributes_test_runner.js */
"use strict";

// One more test testing various add-attributes configurations
// Some of the test data below asserts that long attributes get collapsed

loadHelperScript("helper_attributes_test_runner.js");

/*eslint-disable */
const LONG_ATTRIBUTE = "ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const LONG_ATTRIBUTE_COLLAPSED = "ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEF\u2026UVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const DATA_URL_INLINE_STYLE='color: red; background: url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQAQMAAAAlPW0iAAAABlBMVEUAAAD///+l2Z/dAAAAM0lEQVR4nGP4/5/h/1+G/58ZDrAz3D/McH8yw83NDDeNGe4Ug9C9zwz3gVLMDA/A6P9/AFGGFyjOXZtQAAAAAElFTkSuQmCC");';
const DATA_URL_INLINE_STYLE_COLLAPSED='color: red; background: url("data:image/png;base64,iVBORw0KG\u2026NDDeNGe4Ug9C9zwz3gVLMDA/A6P9/AFGGFyjOXZtQAAAAAElFTkSuQmCC");';
const DATA_URL_ATTRIBUTE = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQAQMAAAAlPW0iAAAABlBMVEUAAAD///+l2Z/dAAAAM0lEQVR4nGP4/5/h/1+G/58ZDrAz3D/McH8yw83NDDeNGe4Ug9C9zwz3gVLMDA/A6P9/AFGGFyjOXZtQAAAAAElFTkSuQmCC";
const DATA_URL_ATTRIBUTE_COLLAPSED = "data:image/png;base64,iVBORw0K\u20269/AFGGFyjOXZtQAAAAAElFTkSuQmCC";
/* eslint-enable */

var TEST_URL = "data:text/html,<div>markup-view attributes addition test</div>";
var TEST_DATA = [{
  desc: "Add an attribute value containing < > &uuml; \" & '",
  text: 'src="somefile.html?param1=<a>&param2=&uuml;&param3=\'&quot;\'"',
  expectedAttributes: {
    src: "somefile.html?param1=<a>&param2=\xfc&param3='\"'"
  }
}, {
  desc: "Add an attribute by clicking the empty space after a node",
  text: 'class="newclass" style="color:green"',
  expectedAttributes: {
    class: "newclass",
    style: "color:green"
  }
}, {
  desc: "Try add an attribute containing a quote (\") attribute by " +
        "clicking the empty space after a node - this should result " +
        "in it being set to an empty string",
  text: 'class="newclass" style="""',
  expectedAttributes: {
    class: "newclass",
    style: ""
  }
}, {
  desc: "Try to add long data URL to make sure it is collapsed in attribute " +
        "editor.",
  text: `style='${DATA_URL_INLINE_STYLE}'`,
  expectedAttributes: {
    "style": DATA_URL_INLINE_STYLE
  },
  validate: (container, inspector) => {
    const editor = container.editor;
    const visibleAttrText = editor.attrElements.get("style")
                                             .querySelector(".attr-value")
                                             .textContent;
    is(visibleAttrText, DATA_URL_INLINE_STYLE_COLLAPSED);
  }
}, {
  desc: "Try to add long attribute to make sure it is collapsed in attribute " +
        "editor.",
  text: `data-long="${LONG_ATTRIBUTE}"`,
  expectedAttributes: {
    "data-long": LONG_ATTRIBUTE
  },
  validate: (container, inspector) => {
    const editor = container.editor;
    const visibleAttrText = editor.attrElements.get("data-long")
                                             .querySelector(".attr-value")
                                             .textContent;
    is(visibleAttrText, LONG_ATTRIBUTE_COLLAPSED);
  }
}, {
  desc: "Try to add long data URL to make sure it is collapsed in attribute " +
        "editor.",
  text: `src="${DATA_URL_ATTRIBUTE}"`,
  expectedAttributes: {
    "src": DATA_URL_ATTRIBUTE
  },
  validate: (container, inspector) => {
    const editor = container.editor;
    const visibleAttrText = editor.attrElements.get("src")
                                .querySelector(".attr-value").textContent;
    is(visibleAttrText, DATA_URL_ATTRIBUTE_COLLAPSED);
  }
}, {
  desc: "Try to add long attribute with collapseAttributes == false" +
  "to make sure it isn't collapsed in attribute editor.",
  text: `data-long="${LONG_ATTRIBUTE}"`,
  expectedAttributes: {
    "data-long": LONG_ATTRIBUTE
  },
  setUp: function(inspector) {
    Services.prefs.setBoolPref("devtools.markup.collapseAttributes", false);
  },
  validate: (container, inspector) => {
    const editor = container.editor;
    const visibleAttrText = editor.attrElements
      .get("data-long")
      .querySelector(".attr-value")
      .textContent;
    is(visibleAttrText, LONG_ATTRIBUTE);
  },
  tearDown: function(inspector) {
    Services.prefs.clearUserPref("devtools.markup.collapseAttributes");
  }
}, {
  desc: "Try to collapse attributes with collapseAttributeLength == 5",
  text: `data-long="${LONG_ATTRIBUTE}"`,
  expectedAttributes: {
    "data-long": LONG_ATTRIBUTE
  },
  setUp: function(inspector) {
    Services.prefs.setIntPref("devtools.markup.collapseAttributeLength", 2);
  },
  validate: (container, inspector) => {
    const firstChar = LONG_ATTRIBUTE[0];
    const lastChar = LONG_ATTRIBUTE[LONG_ATTRIBUTE.length - 1];
    const collapsed = firstChar + "\u2026" + lastChar;
    const editor = container.editor;
    const visibleAttrText = editor.attrElements
      .get("data-long")
      .querySelector(".attr-value")
      .textContent;
    is(visibleAttrText, collapsed);
  },
  tearDown: function(inspector) {
    Services.prefs.clearUserPref("devtools.markup.collapseAttributeLength");
  }
}];

add_task(async function() {
  const {inspector, testActor} = await openInspectorForURL(TEST_URL);
  await runAddAttributesTests(TEST_DATA, "div", inspector, testActor);
});

