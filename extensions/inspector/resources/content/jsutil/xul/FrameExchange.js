/***************************************************************
* FrameExchange ----------------------------------------------
*  Utility for passing specific data to a document loaded
*  through an iframe
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
****************************************************************/

//////////// implementation ////////////////////

var FrameExchange = {
  mData: {},
  mCount: 0,

  loadURL: function(aFrame, aURL, aData)
  {
    var id = "entry-"+this.mCount;
    this.mCount++;
    
    this.mData[id] = aData;

    var url = aURL + "?" + id;
    aFrame.setAttribute("src", url);
  },

  receiveData: function(aWindow)
  {
    var id = aWindow.location.search.substr(1);
    var data = this.mData[id];
    this.mData[id] = null;

    return data;
  }
};


