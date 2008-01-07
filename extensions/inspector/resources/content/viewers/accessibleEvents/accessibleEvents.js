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
*  The viewer for the accessible events occured on a document accessible.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
****************************************************************/

///////////////////////////////////////////////////////////////////////////////
//// Global Variables

var viewer;

///////////////////////////////////////////////////////////////////////////////
//// Global Constants

const kObserverServiceCID = "@mozilla.org/observer-service;1";
const kAccessibleRetrievalCID = "@mozilla.org/accessibleRetrieval;1";

const nsIObserverService = Components.interfaces.nsIObserverService;
const nsIAccessibleRetrieval = Components.interfaces.nsIAccessibleRetrieval;
const nsIAccessibleEvent = Components.interfaces.nsIAccessibleEvent;
const nsIAccessNode = Components.interfaces.nsIAccessNode;

///////////////////////////////////////////////////////////////////////////////
//// Initialization

window.addEventListener("load", AccessibleEventsViewer_initialize, false);

function AccessibleEventsViewer_initialize()
{
  viewer = new AccessibleEventsViewer();
  viewer.initialize(parent.FrameExchange.receiveData(window));
}

///////////////////////////////////////////////////////////////////////////////
//// class AccessibleEventsViewer

function AccessibleEventsViewer()
{
  this.mURL = window.location;
  this.mObsMan = new ObserverManager(this);

  this.mTree = document.getElementById("olAccessibleEvents");
  this.mOlBox = this.mTree.treeBoxObject;
}

AccessibleEventsViewer.prototype =
{
  // initialization

  mSubject: null,
  mPane: null,
  mView: null,

  // interface inIViewer

  get uid() { return "accessibleEvents"; },
  get pane() { return this.mPane; },
  get selection() { return this.mSelection; },

  get subject() { return this.mSubject; },
  set subject(aObject)
  {
    this.mView = new AccessibleEventsView(aObject);
    this.mOlBox.view = this.mView;
    this.mObsMan.dispatchEvent("subjectChange", { subject: aObject });
  },

  initialize: function initialize(aPane)
  {
    this.mPane = aPane;
    aPane.notifyViewerReady(this);
  },

  destroy: function destroy()
  {
    this.mView.destroy();
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

  // utils

  onItemSelected: function onItemSelected()
  {
    var idx = this.mTree.currentIndex;
    this.mSelection = this.mView.getDOMNode(idx);
    this.mObsMan.dispatchEvent("selectionChange",
                               { selection: this.mSelection } );
  },

  clearEventsList: function clearEventsList()
  {
    this.mView.clear();
  }
};

///////////////////////////////////////////////////////////////////////////////
//// AccessibleEventsView

function AccessibleEventsView(aDocument)
{
  this.mDocument = aDocument;
  this.mEvents = [];
  this.mRowCount = 0;

  this.mAccService = XPCU.getService(kAccessibleRetrievalCID,
                                     nsIAccessibleRetrieval);

  this.mAccDocument = this.mAccService.getAccessibleFor(this.mDocument);
  this.mObserverService = XPCU.getService(kObserverServiceCID,
                                          nsIObserverService);

  this.mObserverService.addObserver(this, "accessible-event", false);
}

AccessibleEventsView.prototype = new inBaseTreeView();

AccessibleEventsView.prototype.observe =
function observe(aSubject, aTopic, aData)
{
  var event = XPCU.QI(aSubject, nsIAccessibleEvent);
  var accessible = event.accessible;
  if (!accessible)
    return;

  var accessnode = XPCU.QI(accessible, nsIAccessNode);
  var accDocument = accessnode.accessibleDocument;
  if (accDocument != this.mAccDocument)
    return;

  var type = event.eventType;
  var date = new Date();
  var node = accessnode.DOMNode;

  var eventObj = {
    event: event,
    accessnode: accessnode,
    node: node,
    nodename: node ? node.nodeName : "",
    type: this.mAccService.getStringEventType(type),
    time: date.toLocaleTimeString()
  };

  this.mEvents.unshift(eventObj);
  ++this.mRowCount;
  this.mTree.rowCountChanged(0, 1);
}

AccessibleEventsView.prototype.destroy =
function destroy()
{
  this.mObserverService.removeObserver(this, "accessible-event");
}

AccessibleEventsView.prototype.clear =
function clear()
{
  var count = this.mRowCount;
  this.mRowCount = 0;
  this.mEvents = [];
  this.mTree.rowCountChanged(0, -count);
}

AccessibleEventsView.prototype.getDOMNode =
function getDOMNode(aRow)
{
  var event = this.mEvents[aRow].event;
  var DOMNode = this.mEvents[aRow].node;
  var accessNode = this.mEvents[aRow].accessnode;

  DOMNode.setUserData("accessible", accessNode, null);
  DOMNode.setUserData("accessibleEvent", event, null);
  return DOMNode;
}

AccessibleEventsView.prototype.getCellText =
function getCellText(aRow, aCol)
{
  if (aCol.id == "olcEventType")
    return this.mEvents[aRow].type;
  if (aCol.id == "olcEventTime")
    return this.mEvents[aRow].time;
  if (aCol.id == "olcEventTargetNodeName")
    return this.mEvents[aRow].nodename;
  return "";
}

