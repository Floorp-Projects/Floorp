/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is DOM Node.normalize test code.
 *
 * The Initial Developer of the Original Code is
 * Jeff Walden <jwalden+code@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
