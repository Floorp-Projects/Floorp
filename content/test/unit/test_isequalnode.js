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

  test_isEqualNode_setAttribute();
  test_isEqualNode_clones();
  test_isEqualNode_variety();
  test_isEqualNode_normalization();
  test_isEqualNode_whitespace();
  test_isEqualNode_namespaces();
  test_isEqualNode_wholeDoc();

  // XXX should Node.isEqualNode(null) throw or return false?
  //test_isEqualNode_null();

}

// TEST CODE

var doc, root; // cache for use in all tests

function init()
{
  doc = ParseFile("isequalnode_data.xml");
  root = doc.documentElement;
}

function test_isEqualNode_setAttribute()
{
  // NOTE: 0, 2 are whitespace
  var test1 = doc.getElementById("test_setAttribute");
  var node1 = test1.childNodes.item(1);
  var node2 = test1.childNodes.item(3);

  check_eq_nodes(node1, node2);


  el(node1).setAttribute("bar", "baz");
  check_neq_nodes(node1, node2);

  el(node2).setAttribute("bar", "baz");
  check_eq_nodes(node1, node2);


  // the null namespace is equivalent to no namespace -- section 1.3.3
  // (XML Namespaces) of DOM 3 Core
  node1.setAttributeNS(null, "quux", "17");
  check_neq_nodes(node1, node2);

  node2.setAttribute("quux", "17");
  check_eq_nodes(node1, node2);


  node2.setAttributeNS("http://mozilla.org/", "seamonkey", "rheet");
  check_neq_nodes(node1, node2);

  node1.setAttribute("seamonkey", "rheet");
  check_neq_nodes(node1, node2);

  node1.setAttributeNS("http://mozilla.org/", "seamonkey", "rheet");
  check_neq_nodes(node1, node2);

  // this overwrites the namespaced "seamonkey" attribute added to node2
  // earlier, because this simply sets whatever attribute has the fully
  // qualified name "seamonkey" (the setAttributeNS attribute string wasn't
  // prefixed) -- consequently, node1 and node2 are still unequal
  node2.setAttribute("seamonkey", "rheet");
  check_neq_nodes(node1, node2);
}

function test_isEqualNode_clones()
{
  // tests all elements and attributes in the document
  var all_elts = doc.getElementsByTagName("*");
  for (var i = 0; i < all_elts.length; i++)
  {
    var elt = el(all_elts.item(i));
    check_eq_nodes(elt, elt.cloneNode(true));

    var attrs = elt.attributes;
    for (var j = 0; j < attrs.length; j++)
    {
      var attr = attrs.item(j);
      check_eq_nodes(attr, attr.cloneNode(true));
    }
  }

  var elm = doc.createElement("foo");
  check_eq_nodes(elm, elm.cloneNode(true));
  check_eq_nodes(elm, elm.cloneNode(false));

  elm.setAttribute("fiz", "eit");
  check_eq_nodes(elm, elm.cloneNode(true));
  check_eq_nodes(elm, elm.cloneNode(false));

  elm.setAttributeNS("http://example.com/", "trendoid", "arthroscope");
  check_eq_nodes(elm, elm.cloneNode(true));
  check_eq_nodes(elm, elm.cloneNode(false));

  var elm2 = elm.cloneNode(true);
  check_eq_nodes(elm, elm2);

  const TEXT = "fetishist";

  elm.textContent = TEXT;
  check_neq_nodes(elm, elm2);

  check_neq_nodes(elm, elm.cloneNode(false));
  check_eq_nodes(elm, elm.cloneNode(true));

  elm2.appendChild(doc.createTextNode(TEXT));
  check_eq_nodes(elm, elm2);

  var att = doc.createAttribute("bar");
  check_eq_nodes(att, att.cloneNode(true));
  check_eq_nodes(att, att.cloneNode(false));
}

function test_isEqualNode_variety()
{
  const nodes =
    [
      doc.createElement("foo"),
      doc.createElementNS("http://example.com/", "foo"),
      doc.createElementNS("http://example.org/", "foo"),
      doc.createElementNS("http://example.com/", "FOO"),
      doc.createAttribute("foo", "href='biz'"),
      doc.createAttributeNS("http://example.com/", "foo", "href='biz'"),
      doc.createTextNode("foo"),
      doc.createTextNode("   "),
      doc.createTextNode("    "),
      doc.createComment("foo"),
      doc.createProcessingInstruction("foo", "href='biz'"),
      doc.implementation.createDocumentType("foo", "href='biz'", ""),
      doc.implementation.createDocument("http://example.com/", "foo", null),
      doc.createDocumentFragment()
    ];

  for (var i = 0; i < nodes.length; i++)
  {
    for (var j = i; j < nodes.length; j++)
    {
      if (i == j)
        check_eq_nodes(nodes[i], nodes[j]);
      else
        check_neq_nodes(nodes[i], nodes[j]);
    }
  }
}

function test_isEqualNode_normalization()
{
  var norm = doc.getElementById("test_normalization");
  var node1 = norm.childNodes.item(1);
  var node2 = norm.childNodes.item(3);

  check_eq_nodes(node1, node2);

  node1.appendChild(doc.createTextNode(""));
  check_neq_nodes(node1, node2);

  node1.normalize();
  check_eq_nodes(node1, node2);

  node2.appendChild(doc.createTextNode("fun"));
  node2.appendChild(doc.createTextNode("ctor"));
  node1.appendChild(doc.createTextNode("functor"));
  check_neq_nodes(node1, node2);

  node1.normalize();
  check_neq_nodes(node1, node2);

  node2.normalize();
  check_eq_nodes(node1, node2);

  // reset
  while (node1.hasChildNodes())
    node1.removeChild(node1.childNodes.item(0));
  while (node2.hasChildNodes())
    node2.removeChild(node2.childNodes.item(0));

  // attribute normalization testing

  var at1 = doc.createAttribute("foo");
  var at2 = doc.createAttribute("foo");
  check_eq_nodes(at1, at2);

  // Attr.appendChild isn't implemented yet (bug 56758), so don't run this yet
  if (false)
  {
    at1.appendChild(doc.createTextNode("rasp"));
    at2.appendChild(doc.createTextNode("rasp"));
    check_eq_nodes(at1, at2);

    at1.appendChild(doc.createTextNode(""));
    check_neq_nodes(at1, at2);

    at1.normalize();
    check_eq_nodes(at1, at2);

    at1.appendChild(doc.createTextNode("berry"));
    check_neq_nodes(at1, at2);

    at2.appendChild(doc.createTextNode("ber"));
    check_neq_nodes(at1, at2);

    at2.appendChild(doc.createTextNode("ry"));
    check_neq_nodes(at1, at2);

    at1.normalize();
    check_neq_nodes(at1, at2);

    at2.normalize();
    check_eq_nodes(at1, at2);
  }

  node1.setAttributeNode(at1);
  check_neq_nodes(node1, node2);

  node2.setAttributeNode(at2);
  check_eq_nodes(node1, node2);

  var n1text1 = doc.createTextNode("ratfink");
  var n1elt = doc.createElement("fruitcake");
  var n1text2 = doc.createTextNode("hydrospanner");

  node1.appendChild(n1text1);
  node1.appendChild(n1elt);
  node1.appendChild(n1text2);

  check_neq_nodes(node1, node2);

  var n2text1a = doc.createTextNode("rat");
  var n2text1b = doc.createTextNode("fink");
  var n2elt = doc.createElement("fruitcake");
  var n2text2 = doc.createTextNode("hydrospanner");

  node2.appendChild(n2text1b);
  node2.appendChild(n2elt);
  node2.appendChild(n2text2);
  check_neq_nodes(node1, node2);

  node2.insertBefore(n2text1a, n2text1b);
  check_neq_nodes(node1, node2);

  var tmp_node1 = node1.cloneNode(true);
  tmp_node1.normalize();
  var tmp_node2 = node2.cloneNode(true);
  tmp_node2.normalize();
  check_eq_nodes(tmp_node1, tmp_node2);

  n2elt.appendChild(doc.createTextNode(""));
  check_neq_nodes(node1, node2);

  tmp_node1 = node1.cloneNode(true);
  tmp_node1.normalize();
  tmp_node2 = node2.cloneNode(true);
  tmp_node2.normalize();
  check_eq_nodes(tmp_node1, tmp_node2);

  var typeText1 = doc.createTextNode("type");
  n2elt.appendChild(typeText1);
  tmp_node1 = node1.cloneNode(true);
  tmp_node1.normalize();
  tmp_node2 = node2.cloneNode(true);
  tmp_node2.normalize();
  check_neq_nodes(tmp_node1, tmp_node2);

  n1elt.appendChild(doc.createTextNode("typedef"));
  tmp_node1 = node1.cloneNode(true);
  tmp_node1.normalize();
  tmp_node2 = node2.cloneNode(true);
  tmp_node2.normalize();
  check_neq_nodes(tmp_node1, tmp_node2);
  check_neq_nodes(n1elt, n2elt);

  var typeText2 = doc.createTextNode("def");
  n2elt.appendChild(typeText2);
  tmp_node1 = node1.cloneNode(true);
  tmp_node1.normalize();
  tmp_node2 = node2.cloneNode(true);
  tmp_node2.normalize();
  check_eq_nodes(tmp_node1, tmp_node2);
  check_neq_nodes(node1, node2);

  n2elt.insertBefore(doc.createTextNode(""), typeText2);
  check_neq_nodes(node1, node2);

  n2elt.insertBefore(doc.createTextNode(""), typeText2);
  check_neq_nodes(node1, node2);

  n2elt.insertBefore(doc.createTextNode(""), typeText1);
  check_neq_nodes(node1, node2);

  node1.normalize();
  node2.normalize();
  check_eq_nodes(node1, node2);
}

function test_isEqualNode_whitespace()
{
  equality_check_kids("test_pi1", true);
  equality_check_kids("test_pi2", true);
  equality_check_kids("test_pi3", false);
  equality_check_kids("test_pi4", true);
  equality_check_kids("test_pi5", true);

  equality_check_kids("test_elt1", false);
  equality_check_kids("test_elt2", false);
  equality_check_kids("test_elt3", true);
  equality_check_kids("test_elt4", false);
  equality_check_kids("test_elt5", false);

  equality_check_kids("test_comment1", true);
  equality_check_kids("test_comment2", false);
  equality_check_kids("test_comment3", false);
  equality_check_kids("test_comment4", true);

  equality_check_kids("test_text1", true);
  equality_check_kids("test_text2", false);
  equality_check_kids("test_text3", false);

  equality_check_kids("test_cdata1", false);
  equality_check_kids("test_cdata2", true);
  equality_check_kids("test_cdata3", false);
  equality_check_kids("test_cdata4", false);
  equality_check_kids("test_cdata5", false);
}

function test_isEqualNode_namespaces()
{
  equality_check_kids("test_ns1", false);
  equality_check_kids("test_ns2", false);

  // XXX want more tests here!
}

function test_isEqualNode_null()
{
  check_neq_nodes(doc, null);

  var elts = doc.getElementsByTagName("*");
  for (var i = 0; i < elts.length; i++)
  {
    var elt = elts.item(i);
    check_neq_nodes(elt, null);

    var attrs = elt.attributes;
    for (var j = 0; j < attrs.length; j++)
    {
      var att = attrs.item(j);
      check_neq_nodes(att, null);

      for (var k = 0; k < att.childNodes.length; k++)
      {
        check_neq_nodes(att.childNodes.item(k), null);
      }
    }
  }
}

function test_isEqualNode_wholeDoc()
{
  doc = ParseFile("isequalnode_data.xml");
  var doc2 = ParseFile("isequalnode_data.xml");
  var tw1 =
    doc.createTreeWalker(doc, Components.interfaces.nsIDOMNodeFilter.SHOW_ALL,
                         null, false);
  var tw2 =
    doc2.createTreeWalker(doc2, Components.interfaces.nsIDOMNodeFilter.SHOW_ALL,
                          null, false);
  do {
    check_eq_nodes(tw1.currentNode, tw2.currentNode);
    tw1.nextNode();
  } while(tw2.nextNode());
}

// UTILITY FUNCTIONS

function n(node)  { return node ? node.QueryInterface(nsIDOMNode) : null; }
function el(node) { return node ? node.QueryInterface(nsIDOMElement) : null; }
function at(node) { return node ? node.QueryInterface(nsIDOMAttr) : null; }


// TESTING FUNCTIONS

/**
 * Compares the first and third (zero-indexed) child nodes of the element
 * (typically to allow whitespace) referenced by parentId for isEqualNode
 * equality or inequality based on the value of areEqual.
 *
 * Note that this means that the contents of the element referenced by parentId
 * are whitespace-sensitive, and a stray space introduced during an edit to the
 * file could result in a correct but unexpected (in)equality failure.
 */
function equality_check_kids(parentId, areEqual)
{
  var parent = doc.getElementById(parentId);
  var kid1 = parent.childNodes.item(1);
  var kid2 = parent.childNodes.item(3);

  if (areEqual)
    check_eq_nodes(kid1, kid2);
  else
    check_neq_nodes(kid1, kid2);
}

function check_eq_nodes(n1, n2)
{
  if (n1 && !n1.isEqualNode(n2))
    do_throw(n1 + " should be equal to " + n2);
  if (n2 && !n2.isEqualNode(n1))
    do_throw(n2 + " should be equal to " + n1);
  if (!n1 && !n2)
    do_throw("nodes both null!");
}

function check_neq_nodes(n1, n2)
{
  if (n1 && n1.isEqualNode(n2))
    do_throw(n1 + " should not be equal to " + n2);
  if (n2 && n2.isEqualNode(n1))
    do_throw(n2 + " should not be equal to " + n1);
  if (!n1 && !n2)
    do_throw("n1 and n2 both null!");
}
