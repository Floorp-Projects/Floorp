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

