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
 
 
///////////////////////////////////////////////////////////////////////////////
//// Global Variables

var viewer;

///////////////////////////////////////////////////////////////////////////////
//// Global Constants

const kAccessibleRetrievalCID = "@mozilla.org/accessibleRetrieval;1";

const nsIAccessibleRetrieval = Components.interfaces.nsIAccessibleRetrieval;

const nsIAccessibleEvent = Components.interfaces.nsIAccessibleEvent;
const nsIAccessibleStateChangeEvent =
  Components.interfaces.nsIAccessibleStateChangeEvent;
const nsIAccessibleTextChangeEvent =
  Components.interfaces.nsIAccessibleTextChangeEvent;
const nsIAccessibleCaretMoveEvent =
  Components.interfaces.nsIAccessibleCaretMoveEvent;

///////////////////////////////////////////////////////////////////////////////
//// Initialization/Destruction

window.addEventListener("load", AccessibleEventViewer_initialize, false);

function AccessibleEventViewer_initialize()
{
  viewer = new AccessibleEventViewer();
  viewer.initialize(parent.FrameExchange.receiveData(window));
}

///////////////////////////////////////////////////////////////////////////////
//// class AccessibleEventViewer
function AccessibleEventViewer()
{
  this.mURL = window.location;
  this.mObsMan = new ObserverManager(this);
  this.mAccService = XPCU.getService(kAccessibleRetrievalCID,
                                     nsIAccessibleRetrieval);
}

AccessibleEventViewer.prototype =
{
  mSubject: null,
  mPane: null,
  mAccEventSubject: null,
  mAccService: null,

  get uid() { return "accessibleEvent"; },
  get pane() { return this.mPane; },

  get subject() { return this.mSubject; },
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

    this.mAccEventSubject = this.mSubject.getUserData("accessibleEvent");
    if (!this.mAccEventSubject)
      return;

    XPCU.QI(this.mAccEventSubject, nsIAccessibleEvent);

    var shownPropsId = "";
    if (this.mAccEventSubject instanceof nsIAccessibleStateChangeEvent)
      shownPropsId = "stateChangeEvent";
    else if (this.mAccEventSubject instanceof nsIAccessibleTextChangeEvent)
      shownPropsId = "textChangeEvent";
    else if (this.mAccEventSubject instanceof nsIAccessibleCaretMoveEvent)
      shownPropsId = "caretMoveEvent";

    var props = document.getElementsByAttribute("prop", "*");
    for (var i = 0; i < props.length; i++) {
      var propElm = props[i];
      var isActive = !propElm.hasAttribute("class") ||
                     (propElm.getAttribute("class") == shownPropsId);

      if (isActive) {
        var prop = propElm.getAttribute("prop");
        propElm.textContent = this[prop];
        propElm.parentNode.removeAttribute("hidden");
      } else {
        propElm.parentNode.setAttribute("hidden", "true");
      }
    }
  },

  clearView: function clearView()
  {
    var containers = document.getElementsByAttribute("prop", "*");
    for (var i = 0; i < containers.length; ++i)
      containers[i].textContent = "";
  },

  get isFromUserInput()
  {
    return this.mAccEventSubject.isFromUserInput;
  },

  get state()
  {
    var accessible = this.mAccEventSubject.accessible;

    var stateObj = {value: null};
    var extStateObj = {value: null};
    accessible.getFinalState(stateObj, extStateObj);

    var list = [];

    states = this.mAccService.getStringStates(stateObj.value,
                                              extStateObj.value);

    for (var i = 0; i < states.length; i++)
      list.push(states.item(i));
    return list.join();
  },

  get isEnabled()
  {
    return this.mAccEventSubject.isEnabled();
  },

  get startOffset()
  {
    return this.mAccEventSubject.start;
  },

  get length()
  {
    return this.mAccEventSubject.length;
  },

  get isInserted()
  {
    return this.mAccEventSubject.isInserted();
  },

  get modifiedText()
  {
    return this.mAccEventSubject.modifiedText;
  },

  get caretOffset()
  {
    return this.mAccEventSubject.caretOffset;
  }
};

