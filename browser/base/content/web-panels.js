var panelProgressListener = {
    onProgressChange : function (aWebProgress, aRequest,
                                    aCurSelfProgress, aMaxSelfProgress,
                                    aCurTotalProgress, aMaxTotalProgress) {
    },
    
    onStateChange : function(aWebProgress, aRequest, aStateFlags, aStatus)
    {
        if (!aRequest)
          return;

        //ignore local/resource:/chrome: files
        if (aStatus == NS_NET_STATUS_READ_FROM || aStatus == NS_NET_STATUS_WROTE_TO)
           return;

        const nsIWebProgressListener = Components.interfaces.nsIWebProgressListener;
        const nsIChannel = Components.interfaces.nsIChannel;
        if (aStateFlags & nsIWebProgressListener.STATE_START && 
            aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK) {
            window.parent.document.getElementById('sidebar-throbber').setAttribute("loading", "true");
        }
        else if (aStateFlags & nsIWebProgressListener.STATE_STOP &&
                aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK) {
            window.parent.document.getElementById('sidebar-throbber').removeAttribute("loading");
        }
    }
    ,

    onLocationChange : function(aWebProgress, aRequest, aLocation) {
    },

    onStatusChange : function(aWebProgress, aRequest, aStatus, aMessage) {
    },

    onSecurityChange : function(aWebProgress, aRequest, aState) { 
    },

    QueryInterface : function(aIID)
    {
        if (aIID.equals(Components.interfaces.nsIWebProgressListener) ||
            aIID.equals(Components.interfaces.nsISupportsWeakReference) ||
            aIID.equals(Components.interfaces.nsISupports))
        return this;
        throw Components.results.NS_NOINTERFACE;
    }
};

var gLoadFired = false;
function loadWebPanel(aURI) {
    var panelBrowser = document.getElementById('web-panels-browser');
    panelBrowser.removeAttribute("src");
    panelBrowser.removeAttribute("cachedurl");
    if (!gLoadFired)
      load();
    panelBrowser.setAttribute("src", aURI);
    panelBrowser.setAttribute("cachedurl", aURI);
}

function load()
{
  if (gLoadFired)
    return;

  var panelBrowser = document.getElementById('web-panels-browser');
  panelBrowser.webProgress.addProgressListener(panelProgressListener, Components.interfaces.nsIWebProgress.NOTIFY_ALL);
  if (panelBrowser.getAttribute("cachedurl")) {
    panelBrowser.setAttribute("src", panelBrowser.getAttribute("cachedurl"));
  }
  gLoadFired = true;
}

function unload()
{
  var panelBrowser = document.getElementById('web-panels-browser');
  panelBrowser.webProgress.removeProgressListener(panelProgressListener);
}
