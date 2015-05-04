/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test parseAttribute from node-attribute-parser.js

const Cu = Components.utils;
let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let require = devtools.require;
const {parseAttribute} = require("devtools/shared/node-attribute-parser");

const TEST_DATA = [{
  tagName: "body",
  namespaceURI: "http://www.w3.org/1999/xhtml",
  attributeName: "class",
  attributeValue: "some css class names",
  expected: [
    {value: "some css class names", type: "string"}
  ]
}, {
  tagName: "box",
  namespaceURI: "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
  attributeName: "datasources",
  attributeValue: "/url/1?test=1#test http://mozilla.org/wow",
  expected: [
    {value: "/url/1?test=1#test", type: "uri"},
    {value: " ", type: "string"},
    {value: "http://mozilla.org/wow", type: "uri"}
  ]
}, {
  tagName: "form",
  namespaceURI: "http://www.w3.org/1999/xhtml",
  attributeName: "action",
  attributeValue: "/path/to/handler",
  expected: [
    {value: "/path/to/handler", type: "uri"}
  ]
}, {
  tagName: "a",
  namespaceURI: "http://www.w3.org/1999/xhtml",
  attributeName: "ping",
  attributeValue: "http://analytics.com/track?id=54 http://analytics.com/track?id=55",
  expected: [
    {value: "http://analytics.com/track?id=54", type: "uri"},
    {value: " ", type: "string"},
    {value: "http://analytics.com/track?id=55", type: "uri"}
  ]
}, {
  tagName: "link",
  namespaceURI: "http://www.w3.org/1999/xhtml",
  attributeName: "href",
  attributeValue: "styles.css",
  otherAttributes: [{name: "rel", value: "stylesheet"}],
  expected: [
    {value: "styles.css", type: "cssresource"}
  ]
}, {
  tagName: "link",
  namespaceURI: "http://www.w3.org/1999/xhtml",
  attributeName: "href",
  attributeValue: "styles.css",
  expected: [
    {value: "styles.css", type: "uri"}
  ]
}, {
  tagName: "output",
  namespaceURI: "http://www.w3.org/1999/xhtml",
  attributeName: "for",
  attributeValue: "element-id something id",
  expected: [
    {value: "element-id", type: "idref"},
    {value: " ", type: "string"},
    {value: "something", type: "idref"},
    {value: " ", type: "string"},
    {value: "id", type: "idref"}
  ]
}, {
  tagName: "img",
  namespaceURI: "http://www.w3.org/1999/xhtml",
  attributeName: "contextmenu",
  attributeValue: "id-of-menu",
  expected: [
    {value: "id-of-menu", type: "idref"}
  ]
}, {
  tagName: "img",
  namespaceURI: "http://www.w3.org/1999/xhtml",
  attributeName: "src",
  attributeValue: "omg-thats-so-funny.gif",
  expected: [
    {value: "omg-thats-so-funny.gif", type: "uri"}
  ]
}, {
  tagName: "key",
  namespaceURI: "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
  attributeName: "command",
  attributeValue: "some_command_id",
  expected: [
    {value: "some_command_id", type: "idref"}
  ]
}, {
  tagName: "script",
  namespaceURI: "whatever",
  attributeName: "src",
  attributeValue: "script.js",
  expected: [
    {value: "script.js", type: "jsresource"}
  ]
}];

function run_test() {
  for (let {tagName, namespaceURI, attributeName,
            otherAttributes, attributeValue, expected} of TEST_DATA) {
    do_print("Testing <" + tagName + " " + attributeName + "='" + attributeValue + "'>");

    let attributes = [...otherAttributes||[], {name: attributeName, value: attributeValue}];
    let tokens = parseAttribute(namespaceURI, tagName, attributes, attributeName);
    if (!expected) {
      do_check_true(!tokens);
      continue;
    }

    do_print("Checking that the number of parsed tokens is correct");
    do_check_eq(tokens.length, expected.length);

    for (let i = 0; i < tokens.length; i ++) {
      do_print("Checking the data in token " + i);
      do_check_eq(tokens[i].value, expected[i].value);
      do_check_eq(tokens[i].type, expected[i].type);
    }
  }
}
