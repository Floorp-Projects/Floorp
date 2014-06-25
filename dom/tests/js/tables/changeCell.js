/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function findBody(node)
{
  if (node.nodeType != Node.ELEMENT_NODE) {
    return null;
  }
  var children = node.childNodes;
  if (children == null) {
    return null;
  }
  var length = children.length;
  var child = null;
  var count = 0;
  while (count < length) {
    child = children[count];
    if (child.tagName == "BODY") {
      dump("BODY found");
      return child;
    }
    var body = findBody(child);
    if (null != body) {
      return body;
    }
    count++;
  }
  return null;
}

// Given the body element, find the first table element
function findTable(body)
{
  // XXX A better way to do this would be to use getElementsByTagName(), but
  // it isn't implemented yet...
  var children = body.childNodes
  if (children == null) {
    return null;
  }
  var length = children.length;
  var child = null;
  var count = 0;
  while (count < length) {
    child = children[count];
    if (child.nodeType == Node.ELEMENT_NODE) {
      if (child.tagName == "TABLE") {
        dump("TABLE found");
        break;
      }
    }
    count++;
  }

  return child;
}

// Given a table element, find the first table body
function findTableBody(table)
{
  // XXX A better way to do this would be to use getElementsByTagName(), but
  // it isn't implemented yet...
  var children = table.childNodes
  if (children == null) {
    return null;
  }
  var length = children.length;
  var child = null;
  var count = 0;
  while (count < length) {
    child = children[count];
    if (child.nodeType == Node.ELEMENT_NODE) {
      if (child.tagName == "TBODY") {
        break;
      }
    }
    count++;
  }

  return child;
}

// Change the text of the first table cell
function changeCell(table)
{
  // Get a table body
  var body = findTableBody(table)

  // The table body's first child is a table row
  var row = body.firstChild

  // The row's first child is a table cell
  var cell = row.firstChild

  // Get the cell's text
  var text = cell.firstChild

  // Append some text
  text.append(" NEW TEXT")
}

changeCell(findTable(findBody(document.documentElement)))

