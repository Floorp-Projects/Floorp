/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/***************************************************************
* DNDUtils -------------------------------------------------
*  Utility functions for common drag and drop tasks.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
****************************************************************/

//////////// global variables /////////////////////

var app;

//////////// global constants ////////////////////

////////////////////////////////////////////////////////////////////////////
//// class DNDUtils

var DNDUtils =
{
  invokeSession: function(aTarget, aTypes, aValues)
  {
    var transData, trans, supports;
    var transArray = XPCU.createInstance("@mozilla.org/supports-array;1", "nsISupportsArray");
    for (var i = 0; i < aTypes.length; ++i) {
      transData = this.createTransferableData(aValues[i]);
      trans = XPCU.createInstance("@mozilla.org/widget/transferable;1", "nsITransferable");
      trans.addDataFlavor(aTypes[i]);
      trans.setTransferData (aTypes[i], transData.data, transData.size);
      supports = trans.QueryInterface(Components.interfaces.nsISupports);
      transArray.AppendElement(supports);
    }
    
    var nsIDragService = Components.interfaces.nsIDragService;
    var dragService = XPCU.getService("@mozilla.org/widget/dragservice;1", "nsIDragService");
    dragService.invokeDragSession(aTarget, transArray, null, nsIDragService.DRAGDROP_ACTION_MOVE);
  },
  
  createTransferableData: function(aValue)
  {
    var obj = {};
    if (typeof(aValue) == "string") {
      obj.data = XPCU.createInstance("@mozilla.org/supports-wstring;1", "nsISupportsWString");
      obj.data.data = aValue;
      obj.size = aValue.length*2;
    } else if (false) {
      // TODO: create data for other primitive types
    }
    
    return obj;
  },
  
  checkCanDrop: function(aType)
  {
    var dragService = XPCU.getService("@mozilla.org/widget/dragservice;1", "nsIDragService");
    var dragSession = dragService.getCurrentSession();
    
    var gotFlavor = false;
    for (var i = 0; i < arguments.length; ++i)
      gotFlavor |= dragSession.isDataFlavorSupported(arguments[i]); 

    dragSession.canDrop = gotFlavor;
    return gotFlavor;
  },
  
  getData: function(aType, aIndex)
  {
    var dragService = XPCU.getService("@mozilla.org/widget/dragservice;1", "nsIDragService");
    var dragSession = dragService.getCurrentSession();
    
    var trans = XPCU.createInstance("@mozilla.org/widget/transferable;1", "nsITransferable");
    trans.addDataFlavor(aType);
  
    dragSession.getData(trans, aIndex);
    var data = new Object();
    trans.getAnyTransferData ({}, data, {});
    return data.value;
  }  
  
};

