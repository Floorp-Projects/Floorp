/***************************************************************
* XBLBindings --------------------------------------------
*  The viewer for the computed css styles on a DOM element.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
****************************************************************/

//////////// global variables /////////////////////

var viewer;

//////////// global constants ////////////////////

//////////////////////////////////////////////////

window.addEventListener("load", XBLBindings_initialize, false);

function XBLBindings_initialize()
{
  viewer = new XBLBindings();
  viewer.initialize(parent.FrameExchange.receiveData(window));
}

////////////////////////////////////////////////////////////////////////////
//// class XBLBindings

function XBLBindings()
{
  this.mURL = window.location;
}

XBLBindings.prototype = 
{
  ////////////////////////////////////////////////////////////////////////////
  //// Initialization
  
  mViewee: null,
  mPane: null,

  ////////////////////////////////////////////////////////////////////////////
  //// interface inIViewer

  get uid() { return "computedStyle" },
  get pane() { return this.mPane },

  get viewee() { return this.mViewee },
  set viewee(aObject) 
  {
  },

  initialize: function(aPane)
  {
    this.mPane = aPane;
    aPane.onViewerConstructed(this);
  },

  destroy: function()
  {
  }

};

