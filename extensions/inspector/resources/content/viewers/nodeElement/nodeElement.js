/***************************************************************
* NodeElementViewer --------------------------------------------
*  The default viewer for DOM Nodes
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/util.js
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
*   chrome://inspector/content/jsutil/rdf/RDFU.js
****************************************************************/

//////////// global variables /////////////////////

var viewer;

//////////// global constants ////////////////////

var kDOMDataSourceIID    = "@mozilla.org/rdf/datasource;1?name=Inspector_DOM";

//////////////////////////////////////////////////

window.addEventListener("load", NodeElementViewer_initialize, false);

function NodeElementViewer_initialize()
{
  viewer = new NodeElementViewer();
  viewer.initialize(parent.FrameExchange.receiveData(window));
}

////////////////////////////////////////////////////////////////////////////
//// class NodeElementViewer 

function NodeElementViewer()  // implements inIViewer
{
  this.mURL = window.location;
  this.mAttrTree = document.getElementById("trAttributes");

  var ds = XPCU.createInstance(kDOMDataSourceIID, "nsIInsDOMDataSource");
  this.mDS = ds;
  ds.addFilterByType(2, true);
  this.mAttrTree.database.AddDataSource(ds);
}

NodeElementViewer.prototype = 
{
  ////////////////////////////////////////////////////////////////////////////
  //// Initialization
  
  mDS: null,
  mViewee: null,
  mPane: null,

  get selectedAttribute() { return this.mAttrTree.selectedItems[0] },

  ////////////////////////////////////////////////////////////////////////////
  //// interface inIViewer

  //// attributes 

  get uid() { return "nodeElement" },
  get pane() { return this.mPane },

  get viewee() { return this.mViewee },
  set viewee(aObject) 
  {
    if (aObject.ownerDocument != this.mDS.document)
      this.mDS.document = aObject.ownerDocument;
    this.mViewee = aObject;
    this.setTextValue("nodeName", aObject.nodeName);
    this.setTextValue("nodeType", aObject.nodeType);
    this.setTextValue("nodeValue", aObject.nodeValue);
    this.setTextValue("namespace", aObject.namespaceURI);

    var res = this.mDS.getResourceForObject(aObject);
    try {
      this.mAttrTree.setAttribute("ref", res.Value);
    } catch (ex) {
      debug("ERROR: While rebuilding attribute tree\n" + ex);
    }
  },

  // methods

  initialize: function(aPane)
  {
    this.mPane = aPane;
    aPane.onViewerConstructed(this);
  },

  destroy: function()
  {
    this.mAttrTree.database.RemoveDataSource(this.mDS);
  },

  ////////////////////////////////////////////////////////////////////////////
  //// UI Commands

  cmdNewAttribute: function()
  {
    var name = prompt("Enter the attribute name:", "");
    if (name) {
      var attr = this.mViewee.getAttributeNode(name);
      if (!attr)
        this.mViewee.setAttribute(name, "");
      var res = this.mDS.getResourceForObject(attr);
      var kids = this.mAttrTree.getElementsByTagName("treechildren")[1];
      var item = kids.childNodes[kids.childNodes.length-1];
      if (item) {
        var cell = item.getElementsByAttribute("ins-type", "value")[0];
        this.mAttrTree.selectItem(item);
        this.mAttrTree.startEdit(cell);
      }
    }
  },
  
  cmdEditSelectedAttribute: function()
  {
    var item = this.mAttrTree.selectedItems[0];
    var cell = item.getElementsByAttribute("ins-type", "value")[0];
    this.mAttrTree.startEdit(cell);
  },

  cmdDeleteSelectedAttribute: function()
  {
    var item = this.selectedAttribute;
    if (item) {
      var attrname = InsUtil.getDSProperty(this.mDS, item.id, "nodeName");
      this.mViewee.removeAttribute(attrname);
    }
  },

  cmdSetSelectedAttributeValue: function(aItem, aValue)
  {
    if (aItem) {
      var attrname = InsUtil.getDSProperty(this.mDS, aItem.id, "nodeName");
      this.mViewee.setAttribute(attrname, aValue);
    }
  },

  ////////////////////////////////////////////////////////////////////////////
  //// Uncategorized

  setTextValue: function(aName, aText)
  {
    var field = document.getElementById("tx_"+aName);
    if (field) {
      field.setAttribute("value", aText);
    }
  }

};

