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
