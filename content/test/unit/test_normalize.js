/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test()
{
  /*
   * NOTE: [i] is not allowed in this test, since it's done via classinfo and
   * we don't have that in xpcshell; the workaround is item(i).  Suck.
   */
  init();

  test_element();

  // more tests would be nice here (such as for documents), but the primary
  // uses of Node.normalize() are in test_element; stuff beyond this is either
  // unimplemented or is unlikely to be used all that much within a browser
  // DOM implementation
}

// TEST CODE

var doc; // cache for use in all tests

function init()
{
  doc = ParseFile("empty_document.xml");
}

function test_element()
{
  var x = doc.createElement("funk");

  // one empty Text node
  x.appendChild(doc.createTextNode(""));
  do_check_eq(x.childNodes.length, 1);

  x.normalize();
  do_check_eq(x.childNodes.length, 0);


  // multiple empty Text nodes
  x.appendChild(doc.createTextNode(""));
  x.appendChild(doc.createTextNode(""));
  do_check_eq(x.childNodes.length, 2);

  x.normalize();
  do_check_eq(x.childNodes.length, 0);


  // empty Text node followed by other Text node
  x.appendChild(doc.createTextNode(""));
  x.appendChild(doc.createTextNode("Guaraldi"));
  do_check_eq(x.childNodes.length, 2);

  x.normalize();
  do_check_eq(x.childNodes.length, 1);
  do_check_eq(x.childNodes.item(0).nodeValue, "Guaraldi");


  // Text node followed by empty Text node
  clearKids(x);
  x.appendChild(doc.createTextNode("Guaraldi"));
  x.appendChild(doc.createTextNode(""));
  do_check_eq(x.childNodes.length, 2);

  x.normalize();
  do_check_eq(x.childNodes.item(0).nodeValue, "Guaraldi");


  // Text node followed by empty Text node followed by other Node
  clearKids(x);
  x.appendChild(doc.createTextNode("Guaraldi"));
  x.appendChild(doc.createTextNode(""));
  x.appendChild(doc.createElement("jazzy"));
  do_check_eq(x.childNodes.length, 3);

  x.normalize();
  do_check_eq(x.childNodes.length, 2);
  do_check_eq(x.childNodes.item(0).nodeValue, "Guaraldi");
  do_check_eq(x.childNodes.item(1).nodeName, "jazzy");


  // Nodes are recursively normalized
  clearKids(x);
  var kid = doc.createElement("eit");
  kid.appendChild(doc.createTextNode(""));

  x.appendChild(doc.createTextNode("Guaraldi"));
  x.appendChild(doc.createTextNode(""));
  x.appendChild(kid);
  do_check_eq(x.childNodes.length, 3);
  do_check_eq(x.childNodes.item(2).childNodes.length, 1);

  x.normalize();
  do_check_eq(x.childNodes.length, 2);
  do_check_eq(x.childNodes.item(0).nodeValue, "Guaraldi");
  do_check_eq(x.childNodes.item(1).childNodes.length, 0);
}


// UTILITY FUNCTIONS

function clearKids(node)
{
  while (node.hasChildNodes())
    node.removeChild(node.childNodes.item(0));
}
