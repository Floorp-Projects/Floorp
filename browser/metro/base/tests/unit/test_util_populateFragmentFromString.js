// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource:///modules/ContentUtil.jsm");
let Util = ContentUtil;

function empty_node(node) {
  let cnode;
  while((cnode = node.firstChild))
      node.removeChild(cnode);
  return node;
}

function DOMSerializer() {
  return Components.classes["@mozilla.org/xmlextras/xmlserializer;1"]
          .createInstance(Components.interfaces.nsIDOMSerializer);
}

function serializeContents(node) {
  let str = DOMSerializer().serializeToString(node);
  return str.replace(/^<[^>]+>/, '')
     .replace(/<\/[^>]+>$/, '');
}

function run_test() {
  let doc, body, str, expectedResult, frag;

  do_print("Testing Util.populateFragmentFromString");

  do_check_true(!!Util.populateFragmentFromString);

  do_print("Loading blank document");
  doc = do_parse_document("blank.xhtml", "application/xhtml+xml");

  // sanity check
  do_check_eq(doc.nodeType, 9);
  do_check_true(doc.documentElement.localName != "parsererror");

  body = doc.documentElement.getElementsByTagName("body")[0];
  do_check_eq(1, body.nodeType);

  frag = doc.createDocumentFragment();

  // test proper handling of leading and trailing text
  str = "Before, #2, #1. After";
  Util.populateFragmentFromString(
                                    frag, str,
                                    { text: "One", className: "text-one"},
                                    { text: "Two", className: "text-two"}
                                  );

  empty_node(body);
  body.appendChild(frag);

  expectedResult = 'Before, <span class="text-two">Two</span>, <span class="text-one">One</span>. After';
  do_check_eq(serializeContents(body), expectedResult);

  // test proper handling of unspecified markers
  str = "#8080: #1. http://localhost/#bar";
  Util.populateFragmentFromString(
                                    frag, str,
                                    { text: "One" }
                                  );

  empty_node(body);
  body.appendChild(frag);

  expectedResult = '#8080: <span>One</span>. http://localhost/#bar';
  do_check_eq(serializeContents(body), expectedResult);

  // test proper handling of markup in strings
  str = "#1 <body> tag. &copy; 2000 - Some Corp\u2122";
  Util.populateFragmentFromString(
                                    frag, str,
                                    { text: "About the" }
                                  );

  empty_node(body);
  body.appendChild(frag);
  expectedResult = "<span>About the</span> &lt;body&gt; tag. &amp;copy; 2000 - Some Corp™"
  do_check_eq(serializeContents(body), expectedResult);
}
