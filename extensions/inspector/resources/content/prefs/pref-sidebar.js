/***************************************************************
* SidebarPrefs -------------------------------------------------
*  The controller for the lovely sidebar prefs panel.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
*   chrome://inspector/content/jsutil/rdf/RDFU.js
****************************************************************/

//////////// global variables /////////////////////

var pref;

//////////// global constants ////////////////////

const kDirServiceCID       = "@mozilla.org/file/directory_service;1"
const kNCURI               = "http://home.netscape.com/NC-rdf#";
const kSidebarPanelId      = "UPnls"; // directory services property to find panels.rdf
const kSidebarURNPanelList = "urn:sidebar:current-panel-list";
const kSidebarURN3rdParty  = "urn:sidebar:3rdparty-panel";
const kSidebarURL          = "chrome://inspector/content/sidebar/sidebar.xul";
const kSidebarTitle        = "Document Inspector";

//////////////////////////////////////////////////

window.addEventListener("load", SidebarPrefs_initialize, false);

function SidebarPrefs_initialize()
{
  pref = new SidebarPrefs();
  pref.initSidebarData();
}

///// class SidebarPrefs /////////////////////////

function SidebarPrefs()
{
}

SidebarPrefs.prototype = 
{
  
  init: function()
  {
    var not = this.isSidebarInstalled() ? "" : "Not";
    var el = document.getElementById("bxSidebar"+not+"Installed");
    el.setAttribute("collapsed", "false");
  },

  ///////////////////////////////////////////////////////////////////////////
  // Because nsSidebar has been so mean to me, I'm going to re-write it's
  // addPanel code right here so I don't have to fight with it.  Pbbbbt!
  ///////////////////////////////////////////////////////////////////////////

  initSidebarData: function()
  {
    var file = this.getDirectoryFile(kSidebarPanelId);
    if (file)
      RDFU.loadDataSource(file, gSidebarLoadListener);
  },

  initSidebarData2: function(aDS)
  {
    var res = aDS.GetTarget(gRDF.GetResource(kSidebarURNPanelList), gRDF.GetResource(kNCURI + "panel-list"), true);
    this.mDS = aDS;
    this.mPanelSeq = RDFU.makeSeq(aDS, res);
    this.mPanelRes = gRDF.GetResource(kSidebarURN3rdParty + ":" + kSidebarURL);

    this.init();
  },

  isSidebarInstalled: function()
  {
    return this.mPanelSeq.IndexOf(this.mPanelRes) != -1;
  },

  installSidebar: function()
  {
    if (!this.isSidebarInstalled()) {
      this.mDS.Assert(this.mPanelRes, gRDF.GetResource(kNCURI + "title"), gRDF.GetLiteral(kSidebarTitle), true);
      this.mDS.Assert(this.mPanelRes, gRDF.GetResource(kNCURI + "content"), gRDF.GetLiteral(kSidebarURL), true);
      this.mPanelSeq.AppendElement(this.mPanelRes);
      this.forceSidebarRefresh();
      return true;
    } else
      return false;
  },

  forceSidebarRefresh: function()
  {
    var listRes = gRDF.GetResource(kSidebarURNPanelList);
    var refreshRes = gRDF.GetResource(kNCURI + "refresh");
    var trueRes = gRDF.GetLiteral("true");
    this.mDS.Assert(listRes, refreshRes, trueRes, true);
    this.mDS.Unassert(listRes, refreshRes, trueRes);
  },

  getDirectoryFile: function(aFileId)
  {
    try {
      var dirService = XPCU.getService(kDirServiceCID, "nsIProperties");
      var file = dirService.get(aFileId, Components.interfaces.nsIFile);
      if (!file.exists())
        return null;
      return file.URL;
    } catch (ex) {
      return null;
    }
  }

};

var gSidebarLoadListener = {
  onDataSourceReady: function(aDS) 
  {
    pref.initSidebarData2(aDS);
  },

  onError: function()
  {
  }
};

