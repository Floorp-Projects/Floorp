/***************************************************************
* DOMViewer --------------------------------------------
*  Views all nodes within a document.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/util.js
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
*   chrome://inspector/content/jsutil/rdf/RDFU.js
****************************************************************/

//////////// global variables /////////////////////

var viewer;

var gColumnExtras = {
  "Anonymous": "Anonymous", 
  "NodeType": "NodeType"
};

var gColumnAttrs = {
  "class": "triDOMView"
};

//////////// global constants ////////////////////

var kDOMDataSourceIID    = "@mozilla.org/rdf/datasource;1?name=Inspector_DOM";

//////////////////////////////////////////////////

window.addEventListener("load", DOMViewer_initialize, false);

function DOMViewer_initialize()
{
  viewer = new DOMViewer();
  viewer.initialize(parent.FrameExchange.receiveData(window));
}

////////////////////////////////////////////////////////////////////////////
//// class DOMViewer 

function DOMViewer() // implements inIViewer
{
  this.mDialogs = [];

  this.mDOMTree = document.getElementById("trDOMTree");

  // prepare and attach the DOM DataSource
  this.mDOMDS = XPCU.createInstance(kDOMDataSourceIID, "nsIInsDOMDataSource");
  this.mDOMDS.removeFilterByType(2); // hide attribute nodes
  this.mDOMTree.database.AddDataSource(this.mDOMDS);
}

DOMViewer.prototype = 
{
  mViewee: null,
  mDOMDS: null,
  // searching stuff
  mSearchResults: null,
  mSearchCurrentIdx: null,
  mSearchDirection: null,
  mColumns: null,
  mDialogs: null,
  
  ////////////////////////////////////////////////////////////////////////////
  //// interface inIViewer

  //// attributes 

  get uid() { return "dom" },
  get pane() { return this.mPane },

  get viewee() { return this.mViewee },
  set viewee(aObject) {
    this.mViewee = aObject;
    this.mDOMDS.document = aObject;
    try {
      this.mTreeBuilder.buildContent();
    } catch (ex) {
      debug("ERROR: While rebuilding dom tree\n" + ex);    
    }
  },

  //// methods

  initialize: function(aPane)
  {
    this.initColumns();

    this.mPane = aPane;
    aPane.onViewerConstructed(this);

    this.toggleAnonContent(true, false);
  },
  
  destroy: function()
  {
    for (var i = 0; i < this.mDialogs.length; ++i) {
      this.mDialogs[i].close();
      this.mDialogs[i] = null;
    }
  },

  ////////////////////////////////////////////////////////////////////////////
  //// UI Commands
  
  showFindDialog: function()
  {
    var win = openDialog("chrome://inspector/content/viewers/dom/findDialog.xul", "_blank", "chrome", "id", this);
    this.mDialogs.push(win);
  },

  findNext: function()
  {
    if (this.mSearchResults) {
      this.mSearchCurrentIdx++;
      if (this.mSearchCurrentIdx >= this.mSearchResults.length)
        this.mSearchCurrentIdx = 0;

      this.selectElementInTree(this.mSearchResults[this.mSearchCurrentIdx]);
    }
  },

  toggleAnonContent: function(aExplicit, aValue)
  {
    var val = aExplicit ? aValue : !this.mDOMDS.showAnonymousContent;
    this.mDOMDS.showAnonymousContent = val;
    this.mPane.setCommandAttribute("cmd:toggleAnon", "checked", val ? "true" : "false");
  },
  
  showColumnsDialog: function()
  {
    var win = openDialog("chrome://inspector/content/viewers/dom/columnsDialog.xul", 
      "_blank", "chrome,dependent", this);
    this.mDialogs.push(win);
  },

  selectByClick: function()
  {
    // wait until after user releases the mouse after selecting this command from a UI element
    window.setTimeout("viewer.startSelectByClick()", 10);
  },
  
  startSelectByClick: function()
  {
    var doc = this.mDOMDS.document;
    doc.addEventListener("mousedown", gClickListener, true);
  },

  doSelectByClick: function(aTarget)
  {
    var doc = this.mDOMDS.document;
    doc.removeEventListener("mousedown", gClickListener, true);
    this.selectElementInTree(aTarget);
  },

  onTreeItemSelected: function()
  {
    var item = this.mDOMTree.selectedItems[0];
    this.mPane.onVieweeChanged(this.getNodeFromTreeItem(item));
  },
 
  ////////////////////////////////////////////////////////////////////////////
  //// Searching Methods

  doFindElementById: function(aValue)
  {
    var el = this.mDOMDS.document.getElementById(aValue);
    if (el) {
      this.selectElementInTree(el);
    } else {
      alert("No elements were found.");
    }
  },

  doFindElementsByTagName: function(aValue)
  {
    var els = this.mDOMDS.document.getElementsByTagName(aValue);
    if (els.length == 0) {
      alert("No elements were found.");
    } else {
      this.mSearchResults = els;
      this.mSearchCurrentIdx = 0;
      this.selectElementInTree(els[0]);
    }
  },

  doFindElementsByAttr: function(aAttr, aValue)
  {
    var els = this.mDOMDS.document.getElementsByAttribute(aAttr, aValue);
    if (els.length == 0) {
      alert("No elements were found.");
    } else {
      this.mSearchResults = els;
      this.mSearchCurrentIdx = 0;
      this.selectElementInTree(els[0]);
    }
  },
  
  ///////////////////////////////////////////////////////////////////////////
  // Takes an element from the document being inspected, finds the treeitem
  // which represents it in the DOM tree and selects that treeitem.
  //
  // @param aEl - element from the document being inspected
  ///////////////////////////////////////////////////////////////////////////

  selectElementInTree: function(aEl)
  {
    var searching = true;
    var parent = aEl;
    var line = [];
    var res, item;

    // Keep searching until a pre-created ancestor is
    // found, and then open each ancestor until
    // the found element is created
    while (searching && parent) {
      res = this.mDOMDS.getResourceForObject(parent);
      item = document.getElementById(res.Value);
      line.push(parent);
      if (!item) {
        parent = parent.parentNode;
      } else {
        // we've got all the ancestors, now open them 
        // one-by-one from the top on down
        for (var i = line.length-1; i >= 0; i--) {
          res = this.mDOMDS.getResourceForObject(line[i]);
          item = document.getElementById(res.Value);
          if (!item) return false; // can't find item, so stop trying to descend
          item.setAttribute("open", "true");
        }
        searching = false;
        this.mDOMTree.selectItem(item);
      }
    } 
  },
  
  ////////////////////////////////////////////////////////////////////////////
  //// Columns

  initColumns: function()
  {
    var colPref = PrefUtils.getPref("inspector.dom.columns");
    var cols = colPref.split(",")
    this.mColumns = cols;
    this.mColumnHash = {};
      
    var tb = new inTreeTableBuilder(this.mDOMTree, kInspectorNSURI, "Child");
    tb.allowDragColumns = true;
    tb.isRefContainer = false;
    tb.isContainer = true;
    tb.itemAttributes = gColumnAttrs;
    tb.itemFields = gColumnExtras;
    this.mTreeBuilder = tb;

    tb.initialize();
    
    for (var i = 0; i < cols.length; i++) {
      this.mColumnHash[cols[i]] = true;
      tb.addColumn({
        name: cols[i], 
        title: cols[i],
        flex: i == 0 ? 2 : 1,
        className: "triDOMView"});
    }
    
    // start listening for modifications to columns
    // from drag and drop operations
    tb.onColumnAdd = gColumnAddListener;
    tb.onColumnRemove = gColumnRemoveListener;
    
    tb.build();
  },
  
  hasColumn: function(aName)
  {
    return this.mColumnHash[aName] == true;
  },
  
  //// these add/remove methods depend on names that have already 
  //// been added to the tree builder
  
  doAddColumn: function(aIndex)
  {
    var name = this.mTreeBuilder.getColumnName(aIndex);
    this.mColumnHash[name] = true;
    this.mColumns.splice(aIndex, 0, name);
    this.saveColumns();
  },
  
  doRemoveColumn: function(aIndex)
  {
    var name = this.mColumns[aIndex];
    this.mColumnHash[name] = null;
    this.mColumns.splice(aIndex, 1);
    this.saveColumns();
  },
  
  onColumnAdd: function(aIndex)
  {
    this.doAddColumn(aIndex);
  },
  
  onColumnRemove: function(aIndex)
  {
    this.doRemoveColumn(aIndex);
  },
  
  saveColumns: function()
  {
    var cols = this.mColumns.join(",");
    PrefUtils.setPref("inspector.dom.columns", cols);
  },

  onColumnsDialogReady: function (aDialog)
  {
    this.mColumnsDialog = aDialog;
    this.mTreeBuilder.addColumnDropTarget(aDialog.box);
  },
  
  onColumnsDialogClose: function (aDialog)
  {
    this.mColumnsDialog = null;
  },

  ////////////////////////////////////////////////////////////////////////////
  //// Uncategorized

  getNodeFromTreeItem: function(aItem)
  {
    var res = gRDF.GetResource(aItem.id);
    res = res.QueryInterface(Components.interfaces.nsIDOMDSResource);
    return res ? res.object : null;
  }

};

////////////////////////////////////////////////////////////////////////////
//// Listener Objects

function gClickListener(aEvent)
{
  viewer.doSelectByClick(aEvent.target);
}

function gColumnAddListener(aIndex)
{
  viewer.onColumnAdd(aIndex);
}

function gColumnRemoveListener(aIndex)
{
  viewer.onColumnRemove(aIndex);
}


