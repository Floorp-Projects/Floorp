/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_attributes_test_runner.js */
"use strict";

// Test editing various markup-containers' attribute fields

loadHelperScript("helper_attributes_test_runner.js");

const TEST_URL = URL_ROOT + "doc_markup_edit.html";
var TEST_DATA = [
  {
    desc: "Change an attribute",
    node: "#node1",
    originalAttributes: {
      id: "node1",
      class: "node1",
    },
    name: "class",
    value: 'class="changednode1"',
    expectedAttributes: {
      id: "node1",
      class: "changednode1",
    },
  },
  {
    desc:
      'Try changing an attribute to a quote (") - this should result ' +
      "in it being set to an empty string",
    node: "#node22",
    originalAttributes: {
      id: "node22",
      class: "unchanged",
    },
    name: "class",
    value: 'class="""',
    expectedAttributes: {
      id: "node22",
      class: "",
    },
  },
  {
    desc: "Remove an attribute",
    node: "#node4",
    originalAttributes: {
      id: "node4",
      class: "node4",
    },
    name: "class",
    value: "",
    expectedAttributes: {
      id: "node4",
    },
  },
  {
    desc: "Try add attributes by adding to an existing attribute's entry",
    node: "#node24",
    originalAttributes: {
      id: "node24",
    },
    name: "id",
    value: 'id="node24" class="""',
    expectedAttributes: {
      id: "node24",
      class: "",
    },
  },
];

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);
  await runEditAttributesTests(TEST_DATA, inspector);
});
