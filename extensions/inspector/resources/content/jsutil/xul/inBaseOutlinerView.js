/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 */

/***************************************************************
* inBaseOutlinerView -------------------------------------------------
*  Simple outliner view object meant to be extended.
*  
* Usage example: MyView.prototype = new inBaseOutlinerView();
****************************************************************/

function inBaseOutlinerView() { }

inBaseOutlinerView.prototype = 
{
  mRowCount: 0,
  mOutliner: null,
  
  get rowCount() { return this.mRowCount; },
  setOutliner: function(aOutliner) { this.mOutliner = aOutliner; },
  getCellText: function(aRow, aColId) { return ""; },
  getRowProperties: function(aIndex, aProperties) {},
  getCellProperties: function(aIndex, aColId, aProperties) {},
  getColumnProperties: function(aColId, aColElt, aProperties) {},
  getParentIndex: function(aRowIndex) { },
  hasNextSibling: function(aRowIndex, aAfterIndex) { },
  getLevel: function(aIndex) {},
  isContainer: function(aIndex) {},
  isContainerOpen: function(aIndex) {},
  isContainerEmpty: function(aIndex) {},
  toggleOpenState: function(aIndex) {},
  selectionChanged: function() {},
  cycleHeader: function(aColId, aElt) {},
  cycleCell: function(aRow, aColId) {},
  isEditable: function(aRow, aColId) {},
  setCellText: function(aRow, aColId, aValue) {},
  performAction: function(aAction) {},
  performActionOnRow: function(aAction, aRow) {},
  performActionOnCell: function(aAction, aRow, aColId) {},
  
  // extra utility stuff

  createAtom: function(aVal)
  {
    try {
      var i = Components.interfaces.nsIAtomService;
      var svc = Components.classes["@mozilla.org/atom-service;1"].getService(i);
      return svc.getAtom(aVal);
    } catch(ex) { }
  }
  
};
