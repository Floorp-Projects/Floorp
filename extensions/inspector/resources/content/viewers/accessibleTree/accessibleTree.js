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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

/***************************************************************
* AccessibleEventsViewer --------------------------------------------
*  The viewer for the accessible tree of a document.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
****************************************************************/

///////////////////////////////////////////////////////////////////////////////
//// Global Variables

var viewer;

///////////////////////////////////////////////////////////////////////////////
//// Global Constants

const kAccessibleRetrievalCID = "@mozilla.org/accessibleRetrieval;1";

const nsIAccessibleRetrieval = Components.interfaces.nsIAccessibleRetrieval;
const nsIAccessibleEvent = Components.interfaces.nsIAccessibleEvent;
const nsIAccessible = Components.interfaces.nsIAccessible;
const nsIAccessNode = Components.interfaces.nsIAccessNode;

///////////////////////////////////////////////////////////////////////////////
//// Initialization

window.addEventListener("load", AccessibleTreeViewer_initialize, false);

function AccessibleTreeViewer_initialize()
{
  viewer = new AccessibleTreeViewer();
  viewer.initialize(parent.FrameExchange.receiveData(window));
}

///////////////////////////////////////////////////////////////////////////////
//// AccessibleEventsViewer

function AccessibleTreeViewer()
{
  this.mURL = window.location;
  this.mObsMan = new ObserverManager(this);

  this.mTree = document.getElementById("olAccessibleTree");
  this.mOlBox = this.mTree.treeBoxObject;
}

AccessibleTreeViewer.prototype =
{
  //Initialization

  mSubject: null,
  mPane: null,
  mView: null,

  // interface inIViewer

  get uid() { return "accessibleTree"; },
  get pane() { return this.mPane; },
  get selection() { return this.mSelection; },

  get subject() { return this.mSubject; },
  set subject(aObject)
  {
    this.mView = new inAccTreeView(aObject);
    this.mOlBox.view = this.mView;
    this.mObsMan.dispatchEvent("subjectChange", { subject: aObject });
    this.mView.selection.select(0);
  },

  initialize: function initialize(aPane)
  {
    this.mPane = aPane;
    aPane.notifyViewerReady(this);
  },

  destroy: function destroy()
  {
    this.mOlBox.view = null;
  },

  isCommandEnabled: function isCommandEnabled(aCommand)
  {
    return false;
  },

  getCommand: function getCommand(aCommand)
  {
    return null;
  },

  // event dispatching

  addObserver: function addObserver(aEvent, aObserver)
  {
    this.mObsMan.addObserver(aEvent, aObserver);
  },
  removeObserver: function removeObserver(aEvent, aObserver)
  {
    this.mObsMan.removeObserver(aEvent, aObserver);
  },

  // stuff

  onItemSelected: function onItemSelected()
  {
    var idx = this.mTree.currentIndex;
    this.mSelection = this.mView.getDOMNode(idx);
    this.mObsMan.dispatchEvent("selectionChange",
                               { selection: this.mSelection } );
  }
};

///////////////////////////////////////////////////////////////////////////////
//// inAccTreeView

function inAccTreeView(aDocument)
{
  this.mNodes = [];

  this.mAccService = XPCU.getService(kAccessibleRetrievalCID,
                                     nsIAccessibleRetrieval);

  this.mDocument = aDocument;
  this.mAccDocument = this.mAccService.getAccessibleFor(aDocument);

  var node = this.createNode(this.mAccDocument);
  this.mNodes.push(node);
}

///////////////////////////////////////////////////////////////////////////////
//// inAccTreeView. nsITreeView interface

inAccTreeView.prototype = new inBaseTreeView();

inAccTreeView.prototype.__defineGetter__("rowCount",
function rowCount()
{
  return this.mNodes.length;
});

inAccTreeView.prototype.getCellText =
function getCellText(aRow, aCol)
{
  var node = this.rowToNode(aRow);
  if (!node)
    return "";

  var accessible = node.accessible;

  if (aCol.id == "olcRole")
    return this.mAccService.getStringRole(accessible.finalRole);

  if (aCol.id == "olcName")
    return accessible.name;

  if (aCol.id == "olcNodeName") {
    var node = this.getDOMNodeFor(accessible);
    return node ? node.nodeName : "";
  }

  return "";
}

inAccTreeView.prototype.isContainer =
function isContainer(aRow)
{
  var node = this.rowToNode(aRow);
  return node ? node.isContainer : false;
}

inAccTreeView.prototype.isContainerOpen =
function isContainerOpen(aRow)
{
  var node = this.rowToNode(aRow);
  return node ? node.isOpen : false;
}

inAccTreeView.prototype.isContainerEmpty =
function isContainerEmpty(aRow)
{
  return !this.isContainer(aRow);
}

inAccTreeView.prototype.getLevel =
function getLevel(aRow)
{
  var node = this.rowToNode(aRow);
  return node ? node.level : 0;
}

inAccTreeView.prototype.getParentIndex =
function getParentIndex(aRow)
{
  var node = this.rowToNode(aRow);
  if (!node)
    return -1;

  var checkNode = null;
  var i = aRow - 1;
  do {
    checkNode = this.rowToNode(i);
    if (!checkNode)
      return -1;

    if (checkNode == node.parent)
      return i;
    --i;
  } while (checkNode);

  return -1;
}

inAccTreeView.prototype.hasNextSibling =
function hasNextSibling(aRow, aAfterRow)
{
  var node = this.rowToNode(aRow);
  return node && (node.next != null);
}

inAccTreeView.prototype.toggleOpenState =
function toggleOpenState(aRow)
{
  var node = this.rowToNode(aRow);
  if (!node)
    return;

  var oldCount = this.rowCount;
  if (node.isOpen)
    this.collapseNode(aRow);
  else
    this.expandNode(aRow);

  this.mTree.invalidateRow(aRow);
  this.mTree.rowCountChanged(aRow + 1, this.rowCount - oldCount);
}

///////////////////////////////////////////////////////////////////////////////
//// inAccTreeView. Tree utils.

/**
 * Expands a tree node on the given row.
 *
 * @param aRow - row index.
 */
inAccTreeView.prototype.expandNode =
function expandNode(aRow)
{
  var node = this.rowToNode(aRow);
  if (!node)
    return;

  var kids = node.accessible.children;
  var kidCount = kids.length;

  var newNode = null;
  var prevNode = null;

  for (var i = 0; i < kidCount; ++i) {
    var accessible = kids.queryElementAt(i, nsIAccessible);
    newNode = this.createNode(accessible, node);
    this.mNodes.splice(aRow + i + 1, 0, newNode);

    if (prevNode)
      prevNode.next = newNode;
    newNode.previous = prevNode;
    prevNode = newNode;
  }

  node.isOpen = true;
}

/**
 * Collapse a tree node on the given row.
 *
 * @param aRow - row index.
 */
inAccTreeView.prototype.collapseNode =
function collapseNode(aRow)
{
  var node = this.rowToNode(aRow);
  if (!node)
    return;

  var row = this.getLastDescendantOf(node, aRow);
  this.mNodes.splice(aRow + 1, row - aRow);

  node.isOpen = false;
}

/**
 * Create a tree node.
 *
 * @param aAccessible - an accessible object associated with created tree node.
 * @param aParent - parent tree node for the created tree node.
 * @retrurn - tree node object for the given accesible.
 */
inAccTreeView.prototype.createNode =
function createNode(aAccessible, aParent)
{
  var node = new inAccTreeViewNode(aAccessible);
  node.level = aParent ? aParent.level + 1 : 0;
  node.parent = aParent;
  node.isContainer = aAccessible.children.length > 0;

  return node;
}

/**
 * Return row index of the last node that is a descendant of the given node.
 * If there is no required node then return the given row.
 *
 * @param aNode - tree node for that last descedant is searched.
 * @param aRow - row index of the given tree node.
 */
inAccTreeView.prototype.getLastDescendantOf =
function getLastDescendantOf(aNode, aRow)
{
  var rowCount = this.rowCount;

  var row = aRow + 1;
  for (; row < rowCount; ++row) {
    if (this.mNodes[row].level <= aNode.level)
      return row - 1;
  }

  return rowCount - 1;
}

/**
 * Return a tree node by the given row.
 *
 * @param aRow - row index.
 */
inAccTreeView.prototype.rowToNode =
function rowToNode(aRow)
{
  if (aRow < 0 || aRow >= this.rowCount)
    return null;

  return this.mNodes[aRow];
}

///////////////////////////////////////////////////////////////////////////////
//// inAccTreeView. Accessibility utils.

/**
 * Return DOM node for the given accessible.
 *
 * @param aAccessible - an accessible object.
 */
inAccTreeView.prototype.getDOMNodeFor =
function getDOMNodeFor(aAccessible)
{
  var accessNode = XPCU.QI(aAccessible, nsIAccessNode);
  var DOMNode = accessNode.DOMNode;
  DOMNode.setUserData("accessible", aAccessible, null);
  return DOMNode;
}

/**
 * Return DOM node for an accessible of the tree node pointed by the given
 * row index.
 *
 * @param aRow - row index.
 */
inAccTreeView.prototype.getDOMNode =
function getDOMNode(aRow)
{
  var node = this.mNodes[aRow];
  if (!node)
    return null;

  return this.getDOMNodeFor(node.accessible);
}

///////////////////////////////////////////////////////////////////////////////
//// inAccTreeViewNode

function inAccTreeViewNode(aAccessible)
{
  this.accessible = aAccessible;

  this.parent = null;
  this.next = null;
  this.previous = null;

  this.level = 0;
  this.isOpen = false;
  this.isContainer = false;
}

