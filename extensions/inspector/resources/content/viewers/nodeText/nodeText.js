/***************************************************************
* NodeTextViewer --------------------------------------------
*  The viewer for text nodes.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
****************************************************************/

//////////// global variables /////////////////////

var viewer;

//////////// global constants ////////////////////

//////////////////////////////////////////////////

window.addEventListener("load", NodeTextViewer_initialize, false);

function NodeTextViewer_initialize()
{
  viewer = new NodeTextViewer();
  viewer.initialize(parent.FrameExchange.receiveData(window));
}

////////////////////////////////////////////////////////////////////////////
//// class NodeTextViewer

function NodeTextViewer()
{
  this.mURL = window.location;
}

NodeTextViewer.prototype = 
{
  ////////////////////////////////////////////////////////////////////////////
  //// Initialization
  
  mViewee: null,
  mPane: null,

  ////////////////////////////////////////////////////////////////////////////
  //// interface inIViewer

  get uid() { return "nodeText" },
  get pane() { return this.mPane },

  get viewee() { return this.mViewee },
  set viewee(aObject) 
  {
    this.mViewee = aObject;
    this.setTextValue("NodeValue", aObject.nodeValue);
  },

  initialize: function(aPane)
  {
    this.mPane = aPane;
    aPane.onViewerConstructed(this);
  },

  destroy: function()
  {
  },

  ////////////////////////////////////////////////////////////////////////////
  //// Uncategorized

  setTextValue: function(aName, aText)
  {
    var field = document.getElementById("tx"+aName);
    if (field) {
      field.setAttribute("value", aText);
    }
  }

};
