/***************************************************************
* InspectorSidebar -------------------------------------------------
*  The primary object that controls the Inspector sidebar.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/ViewerRegistry.js
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
*   chrome://inspector/content/jsutil/rdf/RDFArray.js
*   chrome://inspector/content/jsutil/rdf/RDFU.js
*   chrome://inspector/content/jsutil/xul/FrameExchange.js
*   chrome://inspector/content/jsutil/Flasher.js
****************************************************************/

//////////// global variables /////////////////////

var inspector;

var kViewerRegURL          = "chrome/inspector/viewer-registry.rdf";

//////////// global constants ////////////////////

const kXULNSURI = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const kInstallDirId = "CurProcD";

const kFlasherCID          = "@mozilla.org/inspector/flasher;1"
const kWindowMediatorIID   = "@mozilla.org/rdf/datasource;1?name=window-mediator";
const kObserverServiceIID  = "@mozilla.org/observer-service;1";
const kDirServiceCID       = "@mozilla.org/file/directory_service;1"

var gNavigator = window._content;
const nsIPref = Components.interfaces.nsIPref;

//////////////////////////////////////////////////

window.addEventListener("load", InspectorSidebar_initialize, false);

function InspectorSidebar_initialize()
{
  inspector = new InspectorSidebar();
  inspector.initialize();
}

////////////////////////////////////////////////////////////////////////////
//// class InspectorSidebar

function InspectorSidebar() // implements inIViewerPaneContainer
{
  try {
    this.mInstallURL = this.getSpecialDirectory(kInstallDirId).URL;
  } catch (ex) { alert(ex); }
  kViewerRegURL = this.prependBaseURL(kViewerRegURL);

  this.mUFlasher = XPCU.createInstance(kFlasherCID, "inIFlasher");
}

InspectorSidebar.prototype = 
{
  ////////////////////////////////////////////////////////////////////////////
  //// Initialization

  mShell: null,
  mViewerReg: null,
  mPaneCount: 2,
  mCurrentViewer: null,
  mCurrentWindow: null,
  mLoadChecks: 0,
  mInstallURL: null,
  mFlashSelected: null,
  mFlashes: 0,

  get document() { return this.mDocViewerPane.viewer.viewee; },

  initialize: function()
  {
    this.loadViewerRegistry();
    this.installNavObserver();

    this.initPrefs();
    this.setFlashSelected(PrefUtils.getPref("inspector.blink.on"));
  },

  initViewerPanes: function()
  {
    var elPane = document.getElementById("bxDocPane");
    this.mDocViewerPane = new ViewerPane();
    this.mDocViewerPane.initialize("Document", this, elPane, this.mViewerReg);

    elPane = document.getElementById("bxObjectPane");
    this.mObjViewerPane = new ViewerPane();
    this.mObjViewerPane.initialize("Object", this, elPane, this.mViewerReg);

    this.setAllViewerCmdAttributes("disabled", "true");
  },

  ////////////////////////////////////////////////////////////////////////////
  //// interface inIViewerPaneContainer

  get viewerPaneCount() { return this.mPaneCount; },

  onViewerChanged: function(aPane, aOldViewer, aOldEntry, aNewViewer, aNewEntry)
  {
    var ids, el, i;

    this.mViewerReg.cacheViewer(aNewViewer, aNewEntry);

    // disable all commands for for the old viewer
    if (aOldViewer)
      this.setViewerCmdAttribute(aOldEntry, "disabled", "true");

    // enable all commands for for the new viewer
    if (aNewViewer)
      this.setViewerCmdAttribute(aNewEntry, "disabled", "false");
  },
  
  onVieweeChanged: function(aPane, aNewObject)
  {
    if (aPane == this.mDocViewerPane) {
      this.mObjViewerPane.viewee = aNewObject;
      if (this.mFlashSelected)
        this.flashElement(aNewObject, this.mCurrentWindow);
    }
  },

  getViewerPaneAt: function(aIndex)
  {
    if (aIndex == 0) {
      return this.mDocViewerPane;
    } else if (aIndex == 1) {
      return this.mObjViewerPane;
    } else {
      return null;
    }
  },

  setCommandAttribute: function(aCmdId, aAttribute, aValue)
  {
    var cmd = document.getElementById(aCmdId);
    if (cmd)
      cmd.setAttribute(aAttribute, aValue);
  },

  getCommandAttribute: function(aCmdId, aAttribute)
  {
    var cmd = document.getElementById(aCmdId);
    if (cmd)
      return cmd.getAttribute(aAttribute);
  },

  ////////////////////////////////////////////////////////////////////////////
  //// UI Commands

  toggleFlashSelected: function(aExplicit, aValue)
  {
    var val = aExplicit ? aValue : !this.mFlashSelected;
    PrefUtils.setPref("inspector.blink.on", val);
    this.setFlashSelected(val);
  },

  setFlashSelected: function(aValue)
  {
    this.mFlashSelected = aValue;
    var cmd = document.getElementById("cmdFlashSelected");
    cmd.setAttribute("checked", aValue);
  },

  exit: function()
  {
    window.close()
  },

  ////////////////////////////////////////////////////////////////////////////
  //// Viewer Registry Interaction

  loadViewerRegistry: function()
  {
    this.mViewerReg = new ViewerRegistry();
    this.mViewerReg.load(kViewerRegURL, this);
  },

  onViewerRegistryLoad: function()
  {
    this.initViewerPanes();
  },
  
  onViewerRegistryLoadError: function(aStatus, aErrorMsg)
  {
  },

  getViewer: function(aUID)
  {
    return this.mViewerReg.getViewerByUID(aUID);
  },

  ////////////////////////////////////////////////////////////////////////////
  //// History Commands

  prepareHistory: function(aDS)
  {
    this.mHistory = RDFArray.fromContainer(aDS, "inspector:history", kInspectorNSURI);
    var mppHistory = document.getElementById("mppHistory");
    mppHistory.database.AddDataSource(aDS);
    mppHistory.builder.rebuild();
  },

  addToHistory: function(aURL)
  {
    this.mHistory.add({ URL: aURL});
    this.mHistory.save();
  },

  ////////////////////////////////////////////////////////////////////////////
  //// Navigation
  
  setTargetWindow: function(aWindow)
  {
    if (aWindow) {
      this.mCurrentWindow = aWindow;
      this.setTargetDocument(aWindow.document);
    } else
      alert("Unable to switch to window");
  },

  setTargetDocument: function(aDoc)
  {
    this.mDocViewerPane.viewee = aDoc;
  },

  ////////////////////////////////////////////////////////////////////////////
  //// Preferences

  initPrefs: function()
  {
    this.mPrefs = XPCU.getService("@mozilla.org/preferences;1", "nsIPref");
    this.mPrefs.addObserver("inspector", PrefChangeObserver);
  },
  
  setPref: function(aName, aValue)
  {
    var type = this.mPrefs.GetPrefType(aName);
    try {
      if (type == nsIPref.ePrefString) {
        this.mPrefs.SetUnicharPref(aName, aValue);
      } else if (type == nsIPref.ePrefBool) {
        this.mPrefs.SetBoolPref(aName, aValue);
      } else if (type == nsIPref.ePrefInt) {
        this.mPrefs.SetIntPref(aName, aValue);
      }
    } catch(ex) {
      debug("ERROR: Unable to write pref \"" + aName + "\".\n");
    }
  },

  getPref: function(aName)
  {
    var type = this.mPrefs.GetPrefType(aName);
    try {
      if (type == nsIPref.ePrefString) {
        return this.mPrefs.CopyUnicharPref(aName);
      } else if (type == nsIPref.ePrefBool) {
        return this.mPrefs.GetBoolPref(aName);
      } else if (type == nsIPref.ePrefInt) {
        return this.mPrefs.GetIntPref(aName);
      }
    } catch(ex) {
      debug("ERROR: Unable to read pref \"" + aName + "\".\n");
    }
  },

  onPrefChanged: function(aName)
  {
    if (aName == "inspector.blink.on")
      this.setFlashSelected(PrefUtils.getPref("inspector.blink.on"));

    if (this.mFlasher) {
      if (aName == "inspector.blink.border-color") {
        this.mFlasher.color = PrefUtils.getPref("inspector.blink.border-color");
      } else if (aName == "inspector.blink.border-width") {
        this.mFlasher.thickness = PrefUtils.getPref("inspector.blink.border-width");
      } else if (aName == "inspector.blink.duration") {
        this.mFlasher.duration = PrefUtils.getPref("inspector.blink.duration");
      } else if (aName == "inspector.blink.speed") {
        this.mFlasher.speed = PrefUtils.getPref("inspector.blink.speed");
      }
    }
  },
  
  ////////////////////////////////////////////////////////////////////////////
  //// Uncategorized

  fillInTooltip: function(tipElement)
  {
    var retVal = false;
    var textNode = document.getElementById("txTooltip");
    if (textNode) {
      try {  
        var tipText = tipElement.getAttribute("tooltiptext");
        if (tipText != "") {
          textNode.setAttribute("value", tipText);
          retVal = true;
        }
      }
      catch (e) { }
    }
    
    return retVal;
  },

  onTreeItemSelected: function()
  {
    var tree = this.mDOMTree;
    var items = tree.selectedItems;
    if (items.length > 0) {
      var node = this.getNodeFromTreeItem(items[0]);
      this.startViewingNode(node, items[0].id);
    }
  },

  ///////////////////////////////////////////////////////////////////////////
  // The string we get back from shell.getInstallationURL starts with
  // "file://" and so we need to add an extra slash, or the load fails.
  //
  // @param wstring aURL - the url to repair
  ///////////////////////////////////////////////////////////////////////////

  prependBaseURL: function(aURL)
  {
    return "file:///" + this.mInstallURL.substr(7) + aURL;
  },

  flashElement: function(aElement, aWindow)
  {
    // make sure we only try to flash element nodes
    if (aElement.nodeType == 1) {
      if (!this.mFlasher)
        this.mFlasher = new Flasher(this.mUFlasher, 
          PrefUtils.getPref("inspector.blink.border-color"), 
          PrefUtils.getPref("inspector.blink.border-width"), 
          PrefUtils.getPref("inspector.blink.duration"), 
          PrefUtils.getPref("inspector.blink.speed"));

      if (this.mFlasher.flashing) 
        this.mFlasher.stop();
        
      try {
        this.mFlasher.element = aElement;
        this.mFlasher.window = aWindow;
        this.mFlasher.start();
      } catch (ex) {
      }
    }
  },

  setViewerCmdAttribute: function(aEntry, aAttr, aValue)
  {
    var uid = this.mViewerReg.getEntryProperty(aEntry, "uid");
    var cmds = document.getElementById("brsGlobalCommands");
    var els = cmds.getElementsByAttribute("viewer", uid);
    for (var i = 0; i < els.length; i++) {
      if (els[i].getAttribute("exclusive") != "false")
        els[i].setAttribute(aAttr, aValue);
    }
  },

  setAllViewerCmdAttributes: function(aAttr, aValue)
  {
    var count = this.mViewerReg.getEntryCount();
    for (var i = 0; i < count; i++) {
      this.setViewerCmdAttribute(this.mViewerReg.getEntryAt(i), aAttr, aValue);
    }
  },

  getSpecialDirectory: function(aName)
  {
    var dirService = XPCU.getService(kDirServiceCID, "nsIProperties");
    return dirService.get(aName, Components.interfaces.nsIFile);
  },

  installNavObserver: function()
  {
		var observerService = XPCU.getService(kObserverServiceIID, "nsIObserverService");
    observerService.addObserver(NavLoadObserver, "EndDocumentLoad", false);
  }
};

var gHistoryLoadListener = {
  onDataSourceReady: function(aDS) 
  {
    inspector.prepareHistory(aDS);
  },

  onError: function()
  {
  }
};

var NavLoadObserver = {
  Observe: function(aWindow)
  {
    inspector.setTargetWindow(aWindow);
  }
};

var PrefChangeObserver = {
  Observe: function(aSubject, aTopic, aData)
  {
    inspector.onPrefChanged(aData);
  }
};
