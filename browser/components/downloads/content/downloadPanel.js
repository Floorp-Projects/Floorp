const kObserverServiceProgID = "@mozilla.org/observer-service;1";
const NC_NS = "http://home.netscape.com/NC-rdf#";

var gDownloadManager;
var gDownloadHistoryView;

const dlObserver = {
  observe: function(subject, topic, state) {
    if (topic != "dl-progress") return;
    var dl = subject.QueryInterface(Components.interfaces.nsIDownload);
    var elt = document.getElementById(dl.target.path);
    
    var percentComplete = dl.percentComplete;
    if (percentComplete == -1) {
      if (!elt.hasAttribute("progressmode"))
        elt.setAttribute("progressmode", "undetermined");
      if (elt.hasAttribute("progress"))
        elt.removeAttribute("progress");
    }
    else {
      elt.setAttribute("progress", percentComplete);
      if (elt.hasAttribute("progressmode"))
        elt.removeAttribute("progressmode");
    }
  }
};

function Startup() {
  var downloadView = document.getElementById("downloadView");

  const dlmgrContractID = "@mozilla.org/download-manager;1";
  const dlmgrIID = Components.interfaces.nsIDownloadManager;
  gDownloadManager = Components.classes[dlmgrContractID].getService(dlmgrIID);

  var ds = gDownloadManager.datasource;

  downloadView.database.AddDataSource(ds);
  downloadView.builder.rebuild();

  gDownloadHistoryView = document.getElementById("downloadHistoryView");
  gDownloadHistoryView.database.AddDataSource(ds);
  gDownloadHistoryView.builder.rebuild();

  var observerService = Components.classes[kObserverServiceProgID]
                                  .getService(Components.interfaces.nsIObserverService);
  observerService.addObserver(dlObserver, "dl-progress", false);
  observerService.addObserver(dlObserver, "dl-done", false);
  observerService.addObserver(dlObserver, "dl-cancel", false);
  observerService.addObserver(dlObserver, "dl-failed", false);
  
  window.setTimeout(onRebuild, 0);
}

function onRebuild() {
  gDownloadHistoryView.controllers.appendController(downloadViewController);
}

function Shutdown() {
  var observerService = Components.classes[kObserverServiceProgID]
                                  .getService(Components.interfaces.nsIObserverService);
  observerService.removeObserver(dlObserver, "dl-progress");
  observerService.removeObserver(dlObserver, "dl-done");
  observerService.removeObserver(dlObserver, "dl-cancel");
  observerService.removeObserver(dlObserver, "dl-failed");
}

var downloadDNDObserver =
{
  onDragOver: function (aEvent, aFlavour, aDragSession)
  {
    aDragSession.canDrop = true;
  },
  
  onDrop: function(aEvent, aXferData, aDragSession)
  {
    var split = aXferData.data.split("\n");
    var url = split[0];
    var name = split[1];
    saveURL(url, name, null, true, true);
  },
  _flavourSet: null,  
  getSupportedFlavours: function ()
  {
    if (!this._flavourSet) {
      this._flavourSet = new FlavourSet();
      this._flavourSet.appendFlavour("text/x-moz-url");
      this._flavourSet.appendFlavour("text/unicode");
    }
    return this._flavourSet;
  }
}

function onSelect(aEvent) {
  window.updateCommands("list-select");
}
  
var downloadViewController = {
  supportsCommand: function dVC_supportsCommand (aCommand)
  {
    switch (aCommand) {
    case "cmd_properties":
    case "cmd_remove":
    case "cmd_openfile":
    case "cmd_showinshell":
    case "cmd_selectAll":
      return true;
    }
    return false;
  },
  
  isCommandEnabled: function dVC_isCommandEnabled (aCommand)
  {    
    var selectionCount = gDownloadHistoryView.selectedCount;
    if (!selectionCount) return false;

    var selectedItem = gDownloadHistoryView.selectedItem;
    switch (aCommand) {
    case "cmd_openfile":
    case "cmd_showinshell":
    case "cmd_properties":
      return selectionCount == 1;
    case "cmd_remove":
      return selectionCount;
    case "cmd_selectAll":
      return gDownloadHistoryView.getRowCount() != selectionCount;
    default:
      return false;
    }
  },
  
  doCommand: function dVC_doCommand (aCommand)
  {
    var selectedItem, selectedItems, file, i;

    switch (aCommand) {
    case "cmd_openfile":
      selectedItem = gDownloadHistoryView.selectedItem;
      file = getFileForItem(selectedItem);
      file.launch();
      break;
    case "cmd_showinshell":
      selectedItem = gDownloadHistoryView.selectedItem;
      file = getFileForItem(selectedItem);
      
#ifdef XP_UNIX
      // on unix, open a browser window rooted at the parent
      file = file.QueryInterface(Components.interfaces.nsIFile);
      var parent = file.parent;
      if (parent) {
        //XXXBlake use chromeUrlForTask pref here
        const browserURL = "chrome://browser/content/browser.xul";
        window.openDialog(browserURL, "_blank", "chrome,all,dialog=no", parent.path);
      }
#else
      file.reveal();
#endif
      break;
    case "cmd_properties":
      selectedItem = gDownloadHistoryView.selectedItem;
      window.openDialog("chrome://browser/content/downloads/downloadProperties.xul",
                        "_blank", "modal,centerscreen,chrome,resizable=no", selectedItem.id);
      break;
    case "cmd_remove":
      selectedItems = gDownloadHistoryView.selectedItems;
      var selectedIndex = gDownloadHistoryView.selectedIndex;
      gDownloadManager.startBatchUpdate();
      
      // Notify the datasource that we're about to begin a batch operation
      var observer = gDownloadHistoryView.builder.QueryInterface(Components.interfaces.nsIRDFObserver);
      var ds = gDownloadHistoryView.database;
      observer.beginUpdateBatch(ds);
      
      for (i = 0; i <= selectedItems.length - 1; ++i) {
        gDownloadManager.removeDownload(selectedItems[i].id);
      }

      gDownloadManager.endBatchUpdate();
      observer.endUpdateBatch(ds);
      var remote = gDownloadManager.datasource.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
      remote.Flush();
      gDownloadHistoryView.builder.rebuild();
      var rowCount = gDownloadHistoryView.getRowCount();
      if (selectedIndex > ( rowCount- 1))
        selectedIndex = rowCount - 1;

      gDownloadHistoryView.selectedIndex = selectedIndex;
      break;
    case "cmd_selectAll":
      gDownloadHistoryView.selectAll();
      break;
    default:
    }
  },  
  
  onEvent: function dVC_onEvent (aEvent)
  {
    switch (aEvent) {
    case "list-select":
      this.onCommandUpdate();
    }
  },

  onCommandUpdate: function dVC_onCommandUpdate ()
  {
    var cmds = ["cmd_properties", "cmd_remove",
                "cmd_openfile", "cmd_showinshell"];
    for (var command in cmds)
      goUpdateCommand(cmds[command]);
  }
};

function getFileForItem(aElement)
{
  return createLocalFile(aElement.id);
}

function createLocalFile(aFilePath) 
{
  var lfContractID = "@mozilla.org/file/local;1";
  var lfIID = Components.interfaces.nsILocalFile;
  var lf = Components.classes[lfContractID].createInstance(lfIID);
  lf.initWithPath(aFilePath);
  return lf;
}

function buildContextMenu()
{
  var selectionCount = gDownloadHistoryView.selectedCount;
  if (!selectionCount)
    return false;

  var launchItem = document.getElementById("menuitem_launch");
  var launchSep = document.getElementById("menuseparator_launch");
  var removeItem = document.getElementById("menuitem_remove");
  var showItem = document.getElementById("menuitem_show");
  var propsItem = document.getElementById("menuitem_properties");
  var propsSep = document.getElementById("menuseparator_properties");
  showItem.hidden = selectionCount != 1;
  launchItem.hidden = selectionCount != 1;
  launchSep.hidden = selectionCount != 1;
  propsItem.hidden = selectionCount != 1;
  propsSep.hidden = selectionCount != 1;
  return true;
}
    
