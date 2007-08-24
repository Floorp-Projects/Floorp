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
 * The Original Code is DOM Inspector.
 *
 * The Initial Developer of the Original Code is
 * Alexander Surkov.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
 *   Vasiliy Potapenko <vasiliy.potapenko@gmail.com>
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
 
 
///////////////////////////////////////////////////////////////////////////////
//// Global Variables

var viewer;

///////////////////////////////////////////////////////////////////////////////
//// Global Constants

const kAccessibleRetrievalCID = "@mozilla.org/accessibleRetrieval;1";

const nsIAccessibleRetrieval = Components.interfaces.nsIAccessibleRetrieval;
const nsIAccessible = Components.interfaces.nsIAccessible;

const nsIPropertyElement = Components.interfaces.nsIPropertyElement;

///////////////////////////////////////////////////////////////////////////////
//// Initialization/Destruction

window.addEventListener("load", AccessiblePropsViewer_initialize, false);

function AccessiblePropsViewer_initialize()
{
  viewer = new AccessiblePropsViewer();
  viewer.initialize(parent.FrameExchange.receiveData(window));
}

///////////////////////////////////////////////////////////////////////////////
//// class AccessiblePropsViewer
function AccessiblePropsViewer()
{
  this.mURL = window.location;
  this.mObsMan = new ObserverManager(this);
  this.mAccService = XPCU.getService(kAccessibleRetrievalCID,
                                     nsIAccessibleRetrieval);
}

AccessiblePropsViewer.prototype =
{
  mSubject: null,
  mPane: null,
  mAccSubject: null,
  mAccService: null,

  get uid() { return "accessibleProps" },
  get pane() { return this.mPane },

  get subject() { return this.mSubject },
  set subject(aObject)
  {
    this.mSubject = aObject;
    this.updateView();
    this.mObsMan.dispatchEvent("subjectChange", { subject: aObject });
  },

  initialize: function initialize(aPane)
  {
    this.mPane = aPane;
    aPane.notifyViewerReady(this);
  },

  isCommandEnabled: function isCommandEnabled(aCommand)
  {
    return false;
  },
  
  getCommand: function getCommand(aCommand)
  {
    return null;
  },

  destroy: function destroy() {},

  // event dispatching

  addObserver: function addObserver(aEvent, aObserver)
  {
    this.mObsMan.addObserver(aEvent, aObserver);
  },
  removeObserver: function removeObserver(aEvent, aObserver)
  {
    this.mObsMan.removeObserver(aEvent, aObserver);
  },

  // private
  updateView: function updateView()
  {
    this.clearView();

    try {
      this.mAccSubject = this.mSubject.getUserData("accessible");
      if (this.mAccSubject)
        XPCU.QI(this.mAccSubject, nsIAccessible);
      else
        this.mAccSubject = this.mAccService.getAccessibleFor(this.mSubject);
    } catch(e) {
      dump("Failed to get accessible object for node.");
      return;
    }

    var containers = document.getElementsByAttribute("prop", "*");
    for (var i = 0; i < containers.length; ++i) {
      var value = "";
      try {
        var prop = containers[i].getAttribute("prop");
        value = this[prop];
      } catch (e) {
        dump("Accessibility " + prop + " property is not available.\n");
      }

      if (value instanceof Array)
        containers[i].textContent = value.join(", ");
      else
        containers[i].textContent = value;
    }

    var attrs = this.mAccSubject.attributes;
    if (attrs) {
      var enumerate = attrs.enumerate();
      while (enumerate.hasMoreElements())
        this.addAccessibleAttribute(enumerate.getNext());
    }
  },

  addAccessibleAttribute: function addAccessibleAttribute(aElement)
  {
    var prop = XPCU.QI(aElement, nsIPropertyElement);

    var trAttrBody = document.getElementById("trAttrBody");

    var ti = document.createElement("treeitem");
    var tr = document.createElement("treerow");

    var tc = document.createElement("treecell");
    tc.setAttribute("label", prop.key);
    tr.appendChild(tc);

    tc = document.createElement("treecell");
    tc.setAttribute("label", prop.value);
    tr.appendChild(tc);

    ti.appendChild(tr);

    trAttrBody.appendChild(ti);
  },

  removeAccessibleAttributes: function removeAccessibleAttributes()
  {
    var trAttrBody = document.getElementById("trAttrBody");
    while (trAttrBody.hasChildNodes())
      trAttrBody.removeChild(trAttrBody.lastChild)
  },

  clearView: function clearView()
  {
    var containers = document.getElementsByAttribute("prop", "*");
    for (var i = 0; i < containers.length; ++i)
      containers[i].textContent = "";

    this.removeAccessibleAttributes();
  },

  get role()
  {
    return this.mAccService.getStringRole(this.mAccSubject.finalRole);
  },

  get name()
  {
    return this.mAccSubject.name;
  },

  get description()
  {
    return this.mAccSubject.description;
  },

  get value()
  {
    return this.mAccSubject.value;
  },

  get state()
  {
    var stateObj = {value: null};
    var extStateObj = {value: null};
    this.mAccSubject.getFinalState(stateObj, extStateObj);

    var list = [];

    states = this.mAccService.getStringStates(stateObj.value, extStateObj.value);

    for (var i = 0; i < states.length; i++)
      list.push(states.item(i));
    return list;
  },

  get actionNames()
  {
    var list = [];

    var count = this.mAccSubject.numActions;
    for (var i = 0; i < count; i++)
      list.push(this.mAccSubject.getActionName(i));
    return list;
  }
};
