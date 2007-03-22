/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Daniel Glazman (glazman@netscape.com), original author
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

var gContextMenuNode;
var gContextMenuFiringDocumentElement;

function InitStructBarContextMenu(button, docElement)
{
  gContextMenuFiringDocumentElement = docElement;
  gContextMenuNode = button;

  var tag = docElement.nodeName.toLowerCase();

  var structRemoveTag = document.getElementById("structRemoveTag");
  var enableRemove;

  switch (tag) {
    case "body":
    case "tbody":
    case "thead":
    case "tfoot":
    case "col":
    case "colgroup":
    case "tr":
    case "th":
    case "td":
    case "caption":
      enableRemove = false;
      break;
    default:
      enableRemove = true;
      break;
  }
  SetElementEnabled(structRemoveTag, enableRemove);

  var structChangeTag = document.getElementById("structChangeTag");
  SetElementEnabled(structChangeTag, (tag != "body"));
}

function TableCellFilter(node)
{
  switch (node.nodeName.toLowerCase())
    {
    case "td":
    case "th":
    case "caption":
      return NodeFilter.FILTER_ACCEPT;
      break;
    default:
      return NodeFilter.FILTER_SKIP;
      break;
    }
  return NodeFilter.FILTER_SKIP;
}

function StructRemoveTag()
{
  var editor = GetCurrentEditor();
  if (!editor) return;

  var element = gContextMenuFiringDocumentElement;
  var offset = 0;
  var childNodes = element.parentNode.childNodes;

  while (childNodes[offset] != element) {
    ++offset;
  }

  editor.beginTransaction();

  try {

    var tag = element.nodeName.toLowerCase();
    if (tag != "table") {
      MoveChildNodesAfterElement(editor, element, element, offset);
    }
    else {

      var nodeIterator = document.createTreeWalker(element,
                                                   NodeFilter.SHOW_ELEMENT,
                                                   TableCellFilter,
                                                   true);
      var node = nodeIterator.lastChild();
      while (node) {
        MoveChildNodesAfterElement(editor, node, element, offset);
        node = nodeIterator.previousSibling();
      }

    }
    editor.deleteNode(element);
  }
  catch (e) {};

  editor.endTransaction();
}

function MoveChildNodesAfterElement(editor, element, targetElement, targetOffset)
{
  var childNodes = element.childNodes;
  var childNodesLength = childNodes.length;
  var i;
  for (i = childNodesLength - 1; i >= 0; i--) {
    var clone = childNodes.item(i).cloneNode(true);
    editor.insertNode(clone, targetElement.parentNode, targetOffset + 1);
  }
}

function StructChangeTag()
{
  var textbox = document.createElementNS(XUL_NS, "textbox");
  textbox.setAttribute("value", gContextMenuNode.getAttribute("value"));
  textbox.setAttribute("width", gContextMenuNode.boxObject.width);
  textbox.className = "struct-textbox";

  gContextMenuNode.parentNode.replaceChild(textbox, gContextMenuNode);

  textbox.addEventListener("keypress", OnKeyPress, false);
  textbox.addEventListener("blur", ResetStructToolbar, true);

  textbox.select();
}

function StructSelectTag()
{
  SelectFocusNodeAncestor(gContextMenuFiringDocumentElement);
}

function OpenAdvancedProperties()
{
  doAdvancedProperties(gContextMenuFiringDocumentElement);
}

function OnKeyPress(event)
{
  var editor = GetCurrentEditor();

  var keyCode = event.keyCode;
  if (keyCode == 13) {
    var newTag = event.target.value;

    var element = gContextMenuFiringDocumentElement;

    var offset = 0;
    var childNodes = element.parentNode.childNodes;
    while (childNodes.item(offset) != element) {
      offset++;
    }

    editor.beginTransaction();

    try {
      var newElt = editor.document.createElement(newTag);
      if (newElt) {
        childNodes = element.childNodes;
        var childNodesLength = childNodes.length;
        var i;
        for (i = 0; i < childNodesLength; i++) {
          var clone = childNodes.item(i).cloneNode(true);
          newElt.appendChild(clone);
        }
        editor.insertNode(newElt, element.parentNode, offset+1);
        editor.deleteNode(element);
        editor.selectElement(newElt);

        window.content.focus();
      }
    }
    catch (e) {}

    editor.endTransaction();

  }
  else if (keyCode == 27) {
    // if the user hits Escape, we discard the changes
    window.content.focus();
  }
}
