const kObserverServiceProgID = "@mozilla.org/observer-service;1";
var gDownloadView, gDownloadManager;

const dlObserver = {
  observe: function(subject, topic, state) {
    var subject = subject.QueryInterface(Components.interfaces.nsIDownload);
    var elt = document.getElementById(subject.target.path);
    switch (topic) {
      case "dl-progress":
        elt.setAttribute("progress", subject.percentComplete);
        break;
      default:
        elt.onEnd(topic);
        break;
    }
  }
};

function Startup() {
  gDownloadView = document.getElementById("downloadView");
  const dlmgrContractID = "@mozilla.org/download-manager;1";
  const dlmgrIID = Components.interfaces.nsIDownloadManager;
  gDownloadManager = Components.classes[dlmgrContractID].getService(dlmgrIID);
  var ds = gDownloadManager.datasource;
  gDownloadView.database.AddDataSource(ds);
  gDownloadView.builder.rebuild();

  var observerService = Components.classes[kObserverServiceProgID]
                                  .getService(Components.interfaces.nsIObserverService);
  observerService.addObserver(dlObserver, "dl-progress", false);
  observerService.addObserver(dlObserver, "dl-done", false);
  observerService.addObserver(dlObserver, "dl-cancel", false);
  observerService.addObserver(dlObserver, "dl-failed", false);

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
    saveURL(url, name, null, true);
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
