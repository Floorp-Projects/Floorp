/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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
