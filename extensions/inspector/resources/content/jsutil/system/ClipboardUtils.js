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
* ClipboardUtils -------------------------------------------------
*  Utility functions for painless clipboard interaction.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
****************************************************************/

//////////// global variables /////////////////////

//////////// global constants ////////////////////

////////////////////////////////////////////////////////////////////////////
//// class ClipboardUtils

var ClipboardUtils = {

  writeString: function(aString)
  {
    var clipboard = XPCU.getService("@mozilla.org/widget/clipboard;1", "nsIClipboard");
    var transferable = XPCU.createInstance("@mozilla.org/widget/transferable;1", "nsITransferable");
  
    if (clipboard && transferable) {
      transferable.addDataFlavor("text/unicode");
      
      var data = XPCU.createInstance("@mozilla.org/supports-wstring;1", "nsISupportsWString");
  
      if (data) {
        data.data = aString;
        transferable.setTransferData("text/unicode", data, aString.length * 2);
        clipboard.setData(transferable, null, Components.interfaces.nsIClipboard.kGlobalClipboard);
      }
    }
  }

};
