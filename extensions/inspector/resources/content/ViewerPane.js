/***************************************************************
* ViewerPane ---------------------------------------------------
*  Interface for a pane accepts a node and displays all eligible
*  viewers in a list and activates the selected viewer.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
* REQUIRED IMPORTS:
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
****************************************************************/

//////////// global variables /////////////////////

//////////// global constants ////////////////////

////////////////////////////////////////////////////////////////////////////
//// class ViewerPane

function ViewerPane() // implements inIViewerPane
{
}

ViewerPane.prototype =
{
  ////////////////////////////////////////////////////////////////////////////
  //// Initialization

  mContextMenu: null,
  mCurrentEntry: null,
  mCurrentViewer: null,

  ////////////////////////////////////////////////////////////////////////////
  //// interface inIViewerPane

  //// attributes

  get viewee() { return this.mViewee; },
  set viewee(aVal) { this.startViewingObject(aVal) },

  get container() { return this.mContainer; },
  get viewer() { return this.mCurrentViewer; },
  get title() { return this.mTitle; },
  get uiElement() { return this.mUIElement; },
  get viewerReg() { return this.mViewerReg; },

  //// methods

  initialize: function (aTitle, aContainer, aUIElement, aRegistry)
  {
    this.mTitle = aTitle;
    this.mUIElement = aUIElement;
    this.mContainer = aContainer;
    this.mViewerReg = aRegistry;

    this.mListEl = aUIElement.getElementsByAttribute("ins-role", "viewer-list")[0];
    this.mTitleEl = aUIElement.getElementsByAttribute("ins-role", "viewer-title")[0];
    this.mMenuEl = aUIElement.getElementsByAttribute("ins-role", "viewer-menu")[0];
    this.mIFrameEl = aUIElement.getElementsByAttribute("ins-role", "viewer-iframe")[0];

    this.fillViewerList();
  },

  onViewerConstructed: function(aViewer)
  {
    var old = this.mCurrentViewer;
    this.mCurrentViewer = aViewer;
    var oldEntry = this.mCurrentEntry;
    this.mCurrentEntry = this.mPendingEntry;

    this.rebuildViewerContextMenu();

    var title = this.mViewerReg.getEntryProperty(this.mCurrentEntry, "description");
    this.setTitle(title);

    this.mContainer.onViewerChanged(this, old, oldEntry, this.mCurrentViewer, this.mCurrentEntry);

    aViewer.viewee = this.mViewee;
},

  onVieweeChanged: function(aObject)
  {
    this.mContainer.onVieweeChanged(this, aObject);
  },

  setCommandAttribute: function(aCmdId, aAttribute, aValue)
  {
    this.mContainer.setCommandAttribute(aCmdId, aAttribute, aValue);
  },

  getCommandAttribute: function(aCmdId, aAttribute)
  {
    return this.mContainer.getCommandAttribute(aCmdId, aAttribute);
  },

  ////////////////////////////////////////////////////////////////////////////
  //// UI Commands

  ///////////////////////////////////////////////////////////////////////////
  // Sets the new view to the item just selected from the "viewer list"
  ///////////////////////////////////////////////////////////////////////////

  onViewerListCommand: function(aItem)
  {
    this.switchViewer(aItem.entry);

  },

  ///////////////////////////////////////////////////////////////////////////
  // Prepares the list of viewers for a node, rebuilds the menulist to display
  // them, and load the default viewer for the node.
  //
  // @param Object aObject - the object to begin viewing
  ///////////////////////////////////////////////////////////////////////////

  startViewingObject: function(aObject)
  {
    this.mViewee = aObject;

    // get the list of viewers which match the node
    var entries = this.mViewerReg.findViewersForObject(aObject);
    this.rebuildViewerList(entries);

    if (entries.length > 0 && !this.entryInList(this.mCurrentEntry, entries)) {
      this.switchViewer(entries[0]);
    } else {
      this.mCurrentViewer.viewee = aObject;
    }
  },

  ///////////////////////////////////////////////////////////////////////////
  // Clear out and rebuild the menulist full of the available views
  // for the currently selected node.
  //
  // @param Array aEntries - an array of entries from the viewer registry
  ///////////////////////////////////////////////////////////////////////////

  rebuildViewerList: function(aEntries)
  {
    var mpp = this.mListElPopup;

    this.mListEl.setAttribute("disabled", aEntries.length <= 0);

    // empty the list
    while (mpp.childNodes.length)
      mpp.removeChild(mpp.childNodes[0]);

    for (var i = 0; i < aEntries.length; i++) {
      var entry = aEntries[i];
      var menuitem = document.createElement("menuitem");
      menuitem.setAttribute("label", this.mViewerReg.getEntryProperty(entry, "description"));
      menuitem.entry = entry;
      mpp.appendChild(menuitem);
    }
  },

  ///////////////////////////////////////////////////////////////////////////
  // Loads the viewer described by an entry in the viewer registry.
  //
  // @param nsIRDFNode aEntry - entry in the viewer registry
  ///////////////////////////////////////////////////////////////////////////

  switchViewer: function(aEntry)
  {
    var url = this.mViewerReg.getEntryURL(aEntry);

    var loadNew = true;
    if (this.mCurrentViewer) {
      var oldURL = this.mViewerReg.getEntryURL(this.mCurrentEntry);
      if (oldURL == url) {
        loadNew = false;
      }
    }

    if (loadNew) {
      this.mPendingEntry = aEntry;
      this.loadViewerURL(url);
    }
  },

  ///////////////////////////////////////////////////////////////////////////
  // Begin loading a new viewer from a given url.
  //
  // @param String aURL - the url of the viewer document
  ///////////////////////////////////////////////////////////////////////////

  loadViewerURL: function(aURL)
  {
    if (this.mCurrentViewer) {
      // tell the old viewer it's about to go away
      this.mCurrentViewer.destroy();
    }

    // load the new document
    FrameExchange.loadURL(this.mIFrameEl, aURL, this);
  },

  ///////////////////////////////////////////////////////////////////////////
  // Rebuild the viewer context menu
  ///////////////////////////////////////////////////////////////////////////

  rebuildViewerContextMenu: function()
  {
    // remove old context menu
    if (this.mContextMenu) {
      this.mMenuEl.removeChild(this.mContextMenu);
      this.mFormerContextParent.appendChild(this.mContextMenu);
    }

    var uid = this.mViewerReg.getEntryProperty(this.mCurrentEntry, "uid");
    var ppId = "ppViewerContext-" + uid;
    var pp = document.getElementById(ppId);
    if (pp) {
      this.mMenuEl.setAttribute("disabled", "false");
      var parent = pp.parentNode;
      parent.removeChild(pp);
      this.mMenuEl.appendChild(pp);

     this.mFormerContextParent = parent;
     this.mContextMenu = pp;
    } else {
      this.mMenuEl.setAttribute("disabled", "true");
    }
  },

  ///////////////////////////////////////////////////////////////////////////
  // Check to see if an entry exists in an arry of entries
  //
  // @param nsIRDFResource aEntry - the entry being searched for
  // @param Array aList - array of entries
  ///////////////////////////////////////////////////////////////////////////

  entryInList: function(aEntry, aList)
  {
    for (var i in aList) {
      if (aList[i] == aEntry) return true;
    }

    return false;
  },

  ///////////////////////////////////////////////////////////////////////////
  // Set the text in the viewer title bar
  //
  // @param String title - the text to use
  ///////////////////////////////////////////////////////////////////////////

  setTitle: function(aTitle)
  {
    this.mTitleEl.setAttribute("value", (this.mTitle ? this.mTitle + " - " : "") + aTitle);
  },

  ///////////////////////////////////////////////////////////////////////////
  // Fill out the content of the "viewer list" menu
  ///////////////////////////////////////////////////////////////////////////

  fillViewerList: function()
  {
    this.mListEl.pViewer = this;
    this.mListEl.setAttribute("oncommand", "this.pViewer.onViewerListCommand(event.target)");

    var mpp = document.createElement("menupopup");
    this.mListEl.appendChild(mpp);
    this.mListElPopup = mpp;
  }

};


