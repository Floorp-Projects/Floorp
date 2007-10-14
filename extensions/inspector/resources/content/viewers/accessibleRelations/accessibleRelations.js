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
* AccessibleRelationsViewer --------------------------------------------
*  The viewer for the accessible relations for the inspected accessible.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
****************************************************************/

///////////////////////////////////////////////////////////////////////////////
//// Global Variables

var viewer;
var gAccService = null;

///////////////////////////////////////////////////////////////////////////////
//// Global Constants

const kAccessibleRetrievalCID = "@mozilla.org/accessibleRetrieval;1";

const nsIAccessibleRetrieval = Components.interfaces.nsIAccessibleRetrieval;
const nsIAccessibleRelation = Components.interfaces.nsIAccessibleRelation;
const nsIAccessNode = Components.interfaces.nsIAccessNode;
const nsIAccessible = Components.interfaces.nsIAccessible;

///////////////////////////////////////////////////////////////////////////////
//// Initialization

window.addEventListener("load", AccessibleRelationsViewer_initialize, false);

function AccessibleRelationsViewer_initialize()
{
  gAccService = XPCU.getService(kAccessibleRetrievalCID,
                                nsIAccessibleRetrieval);

  viewer = new AccessibleRelationsViewer();
  viewer.initialize(parent.FrameExchange.receiveData(window));
}

///////////////////////////////////////////////////////////////////////////////
//// class AccessibleRelationsViewer

function AccessibleRelationsViewer()
{
  this.mURL = window.location;
  this.mObsMan = new ObserverManager(this);

  this.mTree = document.getElementById("olAccessibleRelations");
  this.mTreeBox = this.mTree.treeBoxObject;

  this.mTargetsTree = document.getElementById("olAccessibleTargets");
  this.mTargetsTreeBox = this.mTargetsTree.treeBoxObject;
}

AccessibleRelationsViewer.prototype =
{
  /////////////////////////
  //// initialization

  mSubject: null,
  mPane: null,
  mView: null,
  mTargetsView: null,

  /////////////////////////
  //// interface inIViewer

  get uid() { return "accessibleRelations"; },
  get pane() { return this.mPane; },
  get selection() { return this.mSelection; },

  get subject() { return this.mSubject; },
  set subject(aObject)
  {
    this.mView = new AccessibleRelationsView(aObject);
    this.mTreeBox.view = this.mView;
    this.mObsMan.dispatchEvent("subjectChange", { subject: aObject });
  },

  initialize: function initialize(aPane)
  {
    this.mPane = aPane;
    aPane.notifyViewerReady(this);
  },

  destroy: function destroy()
  {
    this.mTreeBox.view = null;
    this.mTargetsTreeBox.view = null;
  },

  isCommandEnabled: function isCommandEnabled(aCommand)
  {
    return false;
  },

  getCommand: function getCommand(aCommand)
  {
    return null;
  },

  /////////////////////////
  //// event dispatching

  addObserver: function addObserver(aEvent, aObserver)
  {
    this.mObsMan.addObserver(aEvent, aObserver);
  },
  removeObserver: function removeObserver(aEvent, aObserver)
  {
    this.mObsMan.removeObserver(aEvent, aObserver);
  },

  /////////////////////////
  //// utils

  onItemSelected: function onItemSelected()
  {
    var idx = this.mTree.currentIndex;
    var relation = this.mView.getRelationObject(idx);
    this.mTargetsView = new AccessibleTargetsView(relation);
    this.mTargetsTreeBox.view = this.mTargetsView;
  },

  cmdInspectInNewView: function cmdInspectInNewView()
  {
    var idx = this.mTargetsTree.currentIndex;
    if (idx >= 0) {
      var node = this.mTargetsView.getDOMNode(idx);
      if (node)
        inspectObject(node);
    }
  }
};

///////////////////////////////////////////////////////////////////////////////
//// AccessibleRelationsView

function AccessibleRelationsView(aNode)
{
  this.mNode = aNode;

  this.mAccessible = aNode.getUserData("accessible");
  if (this.mAccessible)
    XPCU.QI(this.mAccessible, nsIAccessible);
  else
    this.mAccessible = gAccService.getAccessibleFor(aNode);

  this.mRelations = this.mAccessible.getRelations();
}

AccessibleRelationsView.prototype = new inBaseTreeView();

AccessibleRelationsView.prototype.__defineGetter__("rowCount",
function rowCount()
{
  return this.mRelations.length;
});

AccessibleRelationsView.prototype.getRelationObject =
function getRelationObject(aRow)
{
  return this.mRelations.queryElementAt(aRow, nsIAccessibleRelation);
}

AccessibleRelationsView.prototype.getCellText =
function getCellText(aRow, aCol)
{
  if (aCol.id == "olcRelationType") {
    var relation = this.getRelationObject(aRow);
    if (relation)
      return gAccService.getStringRelationType(relation.relationType);
  }

  return "";
}

///////////////////////////////////////////////////////////////////////////////
//// AccessibleTargetsView

function AccessibleTargetsView(aRelation)
{
  this.mRelation = aRelation;
  this.mTargets = this.mRelation.getTargets();
}

///////////////////////////////////////////////////////////////////////////////
//// AccessibleTargetsView. nsITreeView

AccessibleTargetsView.prototype = new inBaseTreeView();

AccessibleTargetsView.prototype.__defineGetter__("rowCount",
function rowCount()
{
  return this.mTargets.length;
});

AccessibleTargetsView.prototype.getCellText =
function getCellText(aRow, aCol)
{
  if (aCol.id == "olcRole") {
    var accessible = this.getAccessible(aRow);
    if (accessible)
      return gAccService.getStringRole(accessible.finalRole);
  } else if (aCol.id == "olcNodeName") {
    var node = this.getDOMNode(aRow);
    if (node)
      return node.nodeName;
  }

  return "";
}

///////////////////////////////////////////////////////////////////////////////
//// AccessibleTargetsView. Utils

AccessibleTargetsView.prototype.getAccessible =
function getAccessible(aRow)
{
  return this.mTargets.queryElementAt(aRow, nsIAccessible);
}

AccessibleTargetsView.prototype.getDOMNode =
function getDOMNode(aRow)
{
  var accessNode = this.mTargets.queryElementAt(aRow, nsIAccessNode);
  if (!accessNode)
    return null;

  var DOMNode = accessNode.DOMNode;
  DOMNode.setUserData("accessible", accessNode, null);
  return DOMNode;
}

