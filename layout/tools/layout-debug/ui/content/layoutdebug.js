/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gBrowser;
var gProgressListener;
var gDebugger;
var gRTestIndexList;
var gRTestURLList = null;

const nsILayoutDebuggingTools = Components.interfaces.nsILayoutDebuggingTools;
const nsIDocShell = Components.interfaces.nsIDocShell;
const nsIWebProgressListener = Components.interfaces.nsIWebProgressListener;

const NS_LAYOUT_DEBUGGINGTOOLS_CONTRACTID = "@mozilla.org/layout-debug/layout-debuggingtools;1";


function nsLDBBrowserContentListener()
{
  this.init();
}

nsLDBBrowserContentListener.prototype = {

  init : function()
    {
      this.mStatusText = document.getElementById("status-text");
      this.mURLBar = document.getElementById("urlbar");
      this.mForwardButton = document.getElementById("forward-button");
      this.mBackButton = document.getElementById("back-button");
      this.mStopButton = document.getElementById("stop-button");
    },

  QueryInterface : function(aIID)
    {
      if (aIID.equals(Components.interfaces.nsIWebProgressListener) ||
          aIID.equals(Components.interfaces.nsISupportsWeakReference) ||
          aIID.equals(Components.interfaces.nsISupports))
        return this;
      throw Components.results.NS_NOINTERFACE;
    },

  // nsIWebProgressListener implementation
  onStateChange : function(aWebProgress, aRequest, aStateFlags, aStatus)
    {
      if (!(aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK) ||
          aWebProgress != gBrowser.webProgress)
        return;

      if (aStateFlags & nsIWebProgressListener.STATE_START) {
        this.setButtonEnabled(this.mStopButton, true);
        this.setButtonEnabled(this.mForwardButton, gBrowser.canGoForward);
        this.setButtonEnabled(this.mBackButton, gBrowser.canGoBack);
        this.mStatusText.value = "loading...";
        this.mLoading = true;

      } else if (aStateFlags & nsIWebProgressListener.STATE_STOP) {
        this.setButtonEnabled(this.mStopButton, false);
        this.mStatusText.value = this.mURLBar.value + " loaded";

        if (gRTestURLList && this.mLoading) {
          // Let other things happen in the first 20ms, since this
          // doesn't really seem to be when the page is done loading.
          setTimeout("gRTestURLList.doneURL()", 20);
        }
        this.mLoading = false;
      }
    },

  onProgressChange : function(aWebProgress, aRequest,
                              aCurSelfProgress, aMaxSelfProgress,
                              aCurTotalProgress, aMaxTotalProgress)
    {
    },

  onLocationChange : function(aWebProgress, aRequest, aLocation, aFlags)
    {
      this.mURLBar.value = aLocation.spec;
      this.setButtonEnabled(this.mForwardButton, gBrowser.canGoForward);
      this.setButtonEnabled(this.mBackButton, gBrowser.canGoBack);
    },

  onStatusChange : function(aWebProgress, aRequest, aStatus, aMessage)
    {
      this.mStatusText.value = aMessage;
    },

  onSecurityChange : function(aWebProgress, aRequest, aState)
    {
    },

  // non-interface methods
  setButtonEnabled : function(aButtonElement, aEnabled)
    {
      if (aEnabled)
        aButtonElement.removeAttribute("disabled");
      else
        aButtonElement.setAttribute("disabled", "true");
    },

  mStatusText : null,
  mURLBar : null,
  mForwardButton : null,
  mBackButton : null,
  mStopButton : null,

  mLoading : false

}

function OnLDBLoad()
{
  gBrowser = document.getElementById("browser");

  gProgressListener = new nsLDBBrowserContentListener();
  gBrowser.addProgressListener(gProgressListener);

  gDebugger = Components.classes[NS_LAYOUT_DEBUGGINGTOOLS_CONTRACTID].
                  createInstance(nsILayoutDebuggingTools);

  if (window.arguments && window.arguments[0])
    gBrowser.loadURI(window.arguments[0]);
  else
    gBrowser.goHome();

  gDebugger.init(gBrowser.contentWindow);

  checkPersistentMenus();
  gRTestIndexList = new RTestIndexList();
}

function checkPersistentMenu(item)
{
  var menuitem = document.getElementById("menu_" + item);
  menuitem.setAttribute("checked", gDebugger[item]);
}

function checkPersistentMenus()
{
  // Restore the toggles that are stored in prefs.
  checkPersistentMenu("paintFlashing");
  checkPersistentMenu("paintDumping");
  checkPersistentMenu("invalidateDumping");
  checkPersistentMenu("eventDumping");
  checkPersistentMenu("motionEventDumping");
  checkPersistentMenu("crossingEventDumping");
  checkPersistentMenu("reflowCounts");
}


function OnLDBUnload()
{
  gBrowser.removeProgressListener(gProgressListener);
}

function toggle(menuitem)
{
  // trim the initial "menu_"
  var feature = menuitem.id.substring(5);
  gDebugger[feature] = menuitem.getAttribute("checked") == "true";
}

function openFile()
{
  var nsIFilePicker = Components.interfaces.nsIFilePicker;
  var fp = Components.classes["@mozilla.org/filepicker;1"]
        .createInstance(nsIFilePicker);
  fp.init(window, "Select a File", nsIFilePicker.modeOpen);
  fp.appendFilters(nsIFilePicker.filterHTML | nsIFilePicker.filterAll);
  if (fp.show() == nsIFilePicker.returnOK && fp.fileURL.spec &&
                fp.fileURL.spec.length > 0) {
    gBrowser.loadURI(fp.fileURL.spec);
  }
}
const LDB_RDFNS = "http://mozilla.org/newlayout/LDB-rdf#";
const NC_RDFNS = "http://home.netscape.com/NC-rdf#";

function RTestIndexList() {
  this.init();
}

RTestIndexList.prototype = {

  init : function()
    {
      const nsIPrefService = Components.interfaces.nsIPrefService;
      const PREF_SERVICE_CONTRACTID = "@mozilla.org/preferences-service;1";
      const PREF_BRANCH_NAME = "layout_debugger.rtest_url.";
      const nsIRDFService = Components.interfaces.nsIRDFService;
      const RDF_SERVICE_CONTRACTID = "@mozilla.org/rdf/rdf-service;1";
      const nsIRDFDataSource = Components.interfaces.nsIRDFDataSource;
      const RDF_DATASOURCE_CONTRACTID =
          "@mozilla.org/rdf/datasource;1?name=in-memory-datasource";

      this.mPrefService = Components.classes[PREF_SERVICE_CONTRACTID].
                              getService(nsIPrefService);
      this.mPrefBranch = this.mPrefService.getBranch(PREF_BRANCH_NAME);

      this.mRDFService = Components.classes[RDF_SERVICE_CONTRACTID].
                             getService(nsIRDFService);
      this.mDataSource = Components.classes[RDF_DATASOURCE_CONTRACTID].
                             createInstance(nsIRDFDataSource);

      this.mLDB_Root = this.mRDFService.GetResource(LDB_RDFNS + "Root");
      this.mNC_Name = this.mRDFService.GetResource(NC_RDFNS + "name");
      this.mNC_Child = this.mRDFService.GetResource(NC_RDFNS + "child");

      this.load();

      document.getElementById("menu_RTest_baseline").database.
          AddDataSource(this.mDataSource);
      document.getElementById("menu_RTest_verify").database.
          AddDataSource(this.mDataSource);
      document.getElementById("menu_RTest_remove").database.
          AddDataSource(this.mDataSource);
    },

  save : function()
    {
      this.mPrefBranch.deleteBranch("");

      const nsIRDFLiteral = Components.interfaces.nsIRDFLiteral;
      const nsIRDFResource = Components.interfaces.nsIRDFResource;
      var etor = this.mDataSource.GetTargets(this.mLDB_Root,
                                             this.mNC_Child, true);
      var i = 0;
      while (etor.hasMoreElements()) {
        var resource = etor.getNext().QueryInterface(nsIRDFResource);
        var literal = this.mDataSource.GetTarget(resource, this.mNC_Name, true);
        literal = literal.QueryInterface(nsIRDFLiteral);
        this.mPrefBranch.setCharPref(i.toString(), literal.Value);
        ++i;
      }

      this.mPrefService.savePrefFile(null);
    },

  load : function()
    {
      var prefList = this.mPrefBranch.getChildList("");

      var i = 0;
      for (var pref in prefList) {
        var file = this.mPrefBranch.getCharPref(pref);
        var resource = this.mRDFService.GetResource(file);
        var literal = this.mRDFService.GetLiteral(file);
        this.mDataSource.Assert(this.mLDB_Root, this.mNC_Child, resource, true);
        this.mDataSource.Assert(resource, this.mNC_Name, literal, true);
        ++i;
      }

    },

  /* Add a new list of regression tests to the menus. */
  add : function()
    {
      const nsIFilePicker = Components.interfaces.nsIFilePicker;
      const NS_FILEPICKER_CONTRACTID = "@mozilla.org/filepicker;1";

      var fp = Components.classes[NS_FILEPICKER_CONTRACTID].
                   createInstance(nsIFilePicker);

      // XXX l10n (but this is just for 5 developers, so no problem)
      fp.init(window, "New Regression Test List", nsIFilePicker.modeOpen);
      fp.appendFilters(nsIFilePicker.filterAll);
      fp.defaultString = "rtest.lst";
      if (fp.show() != nsIFilePicker.returnOK)
        return;

      var file = fp.file.persistentDescriptor;
      var resource = this.mRDFService.GetResource(file);
      var literal = this.mRDFService.GetLiteral(file);
      this.mDataSource.Assert(this.mLDB_Root, this.mNC_Child, resource, true);
      this.mDataSource.Assert(resource, this.mNC_Name, literal, true);

      this.save();

    },

  remove : function(file)
    {
      var resource = this.mRDFService.GetResource(file);
      var literal = this.mRDFService.GetLiteral(file);
      this.mDataSource.Unassert(this.mLDB_Root, this.mNC_Child, resource);
      this.mDataSource.Unassert(resource, this.mNC_Name, literal);

      this.save();
    },

  mPrefBranch : null,
  mPrefService : null,
  mRDFService : null,
  mDataSource : null,
  mLDB_Root : null,
  mNC_Child : null,
  mNC_Name : null
}

const nsIFileInputStream = Components.interfaces.nsIFileInputStream;
const nsILineInputStream = Components.interfaces.nsILineInputStream;
const nsIFile = Components.interfaces.nsIFile;
const nsILocalFile = Components.interfaces.nsILocalFile;
const nsIFileURL = Components.interfaces.nsIFileURL;
const nsIIOService = Components.interfaces.nsIIOService;
const nsILayoutRegressionTester = Components.interfaces.nsILayoutRegressionTester;

const NS_LOCAL_FILE_CONTRACTID = "@mozilla.org/file/local;1";
const IO_SERVICE_CONTRACTID = "@mozilla.org/network/io-service;1";
const NS_LOCALFILEINPUTSTREAM_CONTRACTID =
          "@mozilla.org/network/file-input-stream;1";


function RunRTest(aFilename, aIsBaseline, aIsPrinting)
{
  if (gRTestURLList) {
    // XXX Does alert work?
    alert("Already running regression test.\n");
    return;
  }
  dump("Running " + (aIsBaseline?"baseline":"verify") + 
      (aIsPrinting?" PrintMode":"") + " test for " + aFilename + ".\n");

  var listFile = Components.classes[NS_LOCAL_FILE_CONTRACTID].
                    createInstance(nsILocalFile);
  listFile.persistentDescriptor = aFilename;
  gRTestURLList = new RTestURLList(listFile, aIsBaseline, aIsPrinting);
  gRTestURLList.startURL();
}

function RTestURLList(aLocalFile, aIsBaseline, aIsPrinting) {
  this.init(aLocalFile, aIsBaseline, aIsPrinting);
}

RTestURLList.prototype = {
  init : function(aLocalFile, aIsBaseline, aIsPrinting)
    {
      this.mIsBaseline = aIsBaseline;
      this.mIsPrinting = aIsPrinting;
      this.mURLs = new Array();
      this.readFileList(aLocalFile);
      this.mRegressionTester =
        Components.classes["@mozilla.org/layout-debug/regressiontester;1"].
          createInstance(nsILayoutRegressionTester)
    },

  readFileList : function(aLocalFile)
    {
      var ios = Components.classes[IO_SERVICE_CONTRACTID]
                .getService(nsIIOService);
      var dirURL = ios.newFileURI(aLocalFile.parent);

      var fis = Components.classes[NS_LOCALFILEINPUTSTREAM_CONTRACTID].
                    createInstance(nsIFileInputStream);
      fis.init(aLocalFile, -1, -1, false);
      var lis = fis.QueryInterface(nsILineInputStream);

      var line = {value:null};
      do {
        var more = lis.readLine(line);
        var str = line.value;
        str = /^[^#]*/.exec(str); // strip everything after "#"
        str = /\S*/.exec(str); // take the first chunk of non-whitespace
        if (!str || str == "")
          continue;
      
        var item = dirURL.resolve(str);
        if (item.match(/\/rtest.lst$/)) {
          var itemurl = ios.newURI(item, null, null);
          itemurl = itemurl.QueryInterface(nsIFileURL);
          this.readFileList(itemurl.file);
        } else {
          this.mURLs.push( {url:item, dir:aLocalFile.parent, relurl:str} );
        }
      } while (more);
    },

  doneURL : function()
  {
    var basename =
      String(this.mCurrentURL.relurl).replace(/[:=&.\/?]/g, "_") + ".rgd";

    var data = this.mCurrentURL.dir.clone();
    data.append( this.mIsBaseline ? "baseline" : "verify");
    if (!data.exists())
      data.create(nsIFile.DIRECTORY_TYPE, 0o777)
    data.append(basename);

    dump("Writing regression data to " +
         data.QueryInterface(nsILocalFile).persistentDescriptor + "\n");
    if (this.mIsPrinting) {
      this.mRegressionTester.dumpFrameModel(gBrowser.contentWindow, data,
        nsILayoutRegressionTester.DUMP_FLAGS_MASK_PRINT_MODE);
    }
    else {
       this.mRegressionTester.dumpFrameModel(gBrowser.contentWindow, data, 0);
    }
     
      

    if (!this.mIsBaseline) {
      var base_data = this.mCurrentURL.dir.clone();
      base_data.append("baseline");
      base_data.append(basename);
      dump("Comparing to regression data from " +
           base_data.QueryInterface(nsILocalFile).persistentDescriptor + "\n");
      var filesDiffer =
        this.mRegressionTester.compareFrameModels(base_data, data,
          nsILayoutRegressionTester.COMPARE_FLAGS_BRIEF)
      dump("Comparison for " + this.mCurrentURL.url + " " +
           (filesDiffer ? "failed" : "passed") + ".\n");
    }

    this.mCurrentURL = null;

    this.startURL();
  },

  startURL : function()
  {
    this.mCurrentURL = this.mURLs.shift();
    if (!this.mCurrentURL) {
      gRTestURLList = null;
      return;
    }

    gBrowser.loadURI(this.mCurrentURL.url);
  },

  mURLs : null,
  mCurrentURL : null, // url (string), dir (nsIFileURL), relurl (string)
  mIsBaseline : null,
  mRegressionTester : null,
  mIsPrinting : null
}
