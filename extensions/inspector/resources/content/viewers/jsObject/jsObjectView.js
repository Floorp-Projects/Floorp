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
* JSObjectView --------------------------------------------
*  The Outliner View class for the JS Object Viewer.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
****************************************************************/

//////////// global variables /////////////////////

//////////// global constants ////////////////////

////////////////////////////////////////////////////////////////////////////
//// class JSObjectView

function JSObjectView(aObject)
{
  this.mJSObject = aObject;
}

JSObjectView.prototype = 
{
  ////////////////////////////////////////////////////////////////////////////
  //// Initialization
  
  ////////////////////////////////////////////////////////////////////////////
  //// interface nsIOutlinerView

  mOutliner: null,
  mProperties: [],
  mLevels: [],
  mOrder: [],
  
  get rowCount() {
    return this.mObjects.length;
  },
  
/*  get selection() {

  },

  set selection(aVal) {

  },
  
*/  
  getRowProperties: function(aIndex, aProperties)
  {
  },
  
  getCellProperties: function(aIndex, aProperties)
  {
  },
  
  getColumnProperties: function(aColId, aColElt, aProperties)
  {
  },

  isContainer: function(aIndex)
  {
  },
  
  isContainerOpen: function(aIndex)
  {
  },
  
  isContainerEmpty: function(aIndex)
  {
  },
  
  getParentIndex: function(aRowIndex)
  {
    return 1;
  },
  
  hasNextSibling: function(aRowIndex, aAfterIndex)
  {
    return false;
  },
  
  getLevel: function(aIndex)
  {
  },

  getCellText: function(aRow, aColId)
  {
    var object = null;
  
    switch (aColId) {
      case "olrCol1":
        return 1;
        break;
      case "olrCol2":
        return 2;
        break;    
    };
  },
  
  setOutliner: function(aOutliner)
  {
    this.mOutliner = aOutliner;
  },
  
  toggleOpenState: function(aIndex)
  {
  },
  
  cycleHeader: function(aColId, aElt)
  {
  },
  
  selectionChanged: function()
  {
  },
  
  cycleCell: function(aRow, aColId)
  {
  },
  
  isEditable: function(aRow, aColId)
  {
  },
  
  setCellText: function(aRow, aColId, aValue)
  {
  },
  
  performAction: function(aAction)
  {
  },
  
  performActionOnRow: function(aAction, aRow)
  {
  },
  
  performActionOnCell: function(aAction, aRow, aColId)
  {
  },
  
  ////////////////////////////////////////////////////////////////////////////
  //// jsObjectView
  

  
};