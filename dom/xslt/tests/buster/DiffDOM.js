/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ----------------------
// DiffDOM(node1,node2)
// ----------------------

var isHTML = false;

function DiffDOM(node1, node2, aIsHTML)
{
  isHTML = aIsHTML;
  return DiffNodeAndChildren(node1, node2);
}


// namespace attributes in the second node are ignored
const nsreg = /^xmlns[|:\w]/;

// This function does the work of DiffDOM by recursively calling
// itself to explore the tree
function DiffNodeAndChildren(node1, node2)
{
  if (!node1 && !node2)
    return true;
  if (!node1 || !node2)
    return ErrorUp("One of the nodes is null", node1, node2);
  if (node1.type!=node2.type)
    return ErrorUp("Different node types", node1, node2);

  var attributes = node2.attributes;
  if (attributes && attributes.length) {
    var item, name, ns, value, otherValue;
    for (var index = 0; index < attributes.length; index++) {
      item = attributes.item(index);
      ns = item.namespaceURI;
      if (ns) {
        name = item.localName;
        otherValue = node2.getAttributeNS(ns, name);
      }
      else {
        name = item.nodeName;
        otherValue = node2.getAttribute(name);
      }
      value = item.nodeValue;
      if (!nsreg.test(name) && otherValue!=value) {
        return ErrorUp("Different values for attribute", node1, node2);
      }
    }
  }
  else if (node1.attributes && node1.attributes.length) {
    return ErrorUp("Different number of attributes", node1, node2);
  }

  if (isHTML) {
    if (node1.nodeName.toLowerCase()!=node2.nodeName.toLowerCase())
      return ErrorUp("Different node names", node1, node2);
  }
  else {
    if (node1.nodeName!=node2.nodeName)
      return ErrorUp("Different node names", node1, node2);
  }
  if (node1.nodeValue!=node2.nodeValue)
    return ErrorUp("Different node values", node1, node2);
  if (!isHTML)
    if (node1.namespaceURI!=node2.namespaceURI)
      return ErrorUp("Different namespace", node1, node2);
  if (node1.hasChildNodes() != node2.hasChildNodes())
    return ErrorUp("Different children", node1, node2);
  if (node1.childNodes) {
    if (node1.childNodes.length != node2.childNodes.length)
      return ErrorUp("Different number of children", node1, node2);
    for (var child = 0; child < node1.childNodes.length; child++) {
      if (!DiffNodeAndChildren(node1.childNodes[child],
                               node2.childNodes[child])) {
        return false;
      }
    }
  }
  return true;
}

function ErrorUp(errMsg, node1, node2)
{
  dump("Error: "+errMsg+"\n");
  if (node1) {
      dump("Node 1: "+node1+", ");
      if (node1.nodeType == Node.TEXT_NODE)
          dump("nodeValue: "+node1.nodeValue+"\n");
      else
          dump("nodeName: "+node1.namespaceURI+":"+node1.nodeName+"\n");
  }
  if (node2) {
      dump("Node 2: "+node2+", ");
      if (node2.nodeType == Node.TEXT_NODE)
          dump("nodeValue: "+node2.nodeValue+"\n");
      else
          dump("nodeName: "+node2.namespaceURI+":"+node2.nodeName+"\n");
  }
  return false;
}
