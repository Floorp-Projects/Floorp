/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Ben Goodger <ben@netscape.com> (Original Author)
 */
 
/** 
 * This is a non-extensible palette. It will do for now.
 */
var vxPalette = 
{
  // we want to use controllers, but this will do to test our txns.
  mElementCount: { },
  
  insertButtonElement: function (aType) 
  {
    var nElements = this.incrementElementCount("button");
    var attributes = ["value", "id"];
    var values = ["Button " + nElements, "button_" + nElements];
    this.insertSingleElement("button", attributes, values, "Insert Button");    
  },

  insertTextFieldElement: function (aType)
  {
    var nElements = this.incrementElementCount("textfield");
    var attributes = ["value", "id"];
    var values = ["Textfield " + nElements, "textfield_" + nElements];
    this.insertSingleElement("textfield", attributes, values, "Insert Textfield");
  },

  insertRadioGroup: function ()
  {
    var nRadios = this.incrementElementCount("radio");
    var nRadiogroups = this.incrementElementCount("radiogroup");

    var vixenMain = vxUtils.getWindow("vixen:main");
    var focusedWindow = vixenMain.vxShell.mFocusedWindow;
    
    // need to check to see if the focused window is a vfd

    var insertionPoint = focusedWindow.vxVFD.getInsertionPoint();
    
    var vfdDocument = focusedWindow.vxVFD.getContent(true).document;
   
    var radiogroupTxn = new vxCreateElementTxn(vfdDocument, "radiogroup", insertionPoint.parent, insertionPoint.index);
    radiogroupTxn.init();
    var radiogroupAttributes = ["id"];
    var radiogroupValues = ["radiogroup_" + nRadiogroups];
    var radiogroupAttrTxn = new vxChangeAttributeTxn(radiogroupTxn.mID, 
                                                     radiogroupAttributes, 
                                                     radiogroupValues, false);
    radiogroupAttrTxn.init();
    radiogroupTxn.addListener(radiogroupAttrTxn);

    var radioTxn = new vxCreateElementTxn(vfdDocument, "radio", radiogroupTxn.mID, insertionPoint.index);
    radioTxn.init();
    radiogroupTxn.addListener(radioTxn);
    
    var radioAttributes = ["value", "group", "id"];
    var radioValues = ["Radio " + nRadios, "radiogroup_" + nRadiogroups, "radio_" + nRadios];
    var radioAttrTxn = new vxChangeAttributeTxn(radioTxn.mID, 
                                                radioAttributes, 
                                                radioValues, false);
    radioAttrTxn.init();
    radioTxn.addListener(radioAttrTxn);

    // batch the transactions
    var txns = [radiogroupTxn, radiogroupAttrTxn, radioTxn, radioAttrTxn];
    var aggregateTxn = new vxAggregateTxn(txns);
    aggregateTxn.init();
    aggregateTxn.description = "Insert Radiogroup";
    
    var txmgr = focusedWindow.vxVFD.mTxMgrShell;
    txmgr.doTransaction(aggregateTxn);
  },
  
  incrementElementCount: function (aNodeName)
  {
    // create an node count entry for the specified node name
    if (!(aNodeName in this.mElementCount))
      this.mElementCount[aNodeName] = 0;
    return ++this.mElementCount[aNodeName];
  },
  
  insertSingleElement: function (aNodeName, aAttributes, aValues, aDescription)
  {
    var vixenMain = vxUtils.getWindow("vixen:main");
    var focusedWindow = vixenMain.vxShell.mFocusedWindow;
    
    // need to check to see if the focused window is a vfd

    var insertionPoint = focusedWindow.vxVFD.getInsertionPoint();
    var vfdDocument = focusedWindow.vxVFD.getContent(true).document;

    var elementTxn = new vxCreateElementTxn(vfdDocument, aNodeName, insertionPoint.parent, insertionPoint.index);
    elementTxn.init();
    var elementAttrTxn = new vxChangeAttributeTxn(elementTxn.mID, aAttributes, aValues, false);
    elementAttrTxn.init();
    elementTxn.addListener(elementAttrTxn);
      
    // batch the transactions
    var aggregateTxn = new vxAggregateTxn([elementTxn, elementAttrTxn]);
    aggregateTxn.init();
    aggregateTxn.description = aDescription;
    
    var txmgr = focusedWindow.vxVFD.mTxMgrShell;
    txmgr.doTransaction(aggregateTxn);
  },

  /** 
   * Implements nsITransactionListener
   */
  didUndo: function (aTransactionManager, aTransaction, aInterrupt)
  {
    // Register the palette as a transaction listener so that the instance
    // count for each type of widget can be decremented when a transaction
    // is done. 
  }
};

_dd("read vxPalette.js");

