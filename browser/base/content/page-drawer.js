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
            parent.document.getElementById('sidebar-throbber').setAttribute("loading", "true");
        }
        else if (aStateFlags & nsIWebProgressListener.STATE_STOP &&
                aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK) {
            parent.document.getElementById('sidebar-throbber').removeAttribute("loading");
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

var panelAreaDNDObserver = {
  onDrop: function (aEvent, aXferData, aDragSession)
    {
      var url = transferUtils.retrieveURLFromData(aXferData.data, aXferData.flavour.contentType);
      
      // valid urls don't contain spaces ' '; if we have a space it isn't a valid url so bail out
      if (!url || !url.length || url.indexOf(" ", 0) != -1) 
        return;

      var uri = getShortcutOrURI(url);
      try {
        document.getElementById('pageholder-browser').webNavigation.loadURI(uri, nsIWebNavigation.LOAD_FLAGS_NONE, null, null, null);
        var addButton = document.getElementById('addpanel-button');
        addButton.disabled = false;
      } catch (e) {}

      // keep the event from being handled by the dragDrop listeners
      // built-in to gecko if they happen to be above us.    
      aEvent.preventDefault();
    },

  getSupportedFlavours: function ()
    {
      var flavourSet = new FlavourSet();
      flavourSet.appendFlavour("text/x-moz-url");
      flavourSet.appendFlavour("text/unicode");
      flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
      return flavourSet;
    }
  
};

function load()
{
  document.getElementById('pageholder-browser').webProgress.addProgressListener(panelProgressListener, Components.interfaces.nsIWebProgress.NOTIFY_ALL);
  loadPlaceholderPage();
}

function unload()
{
  document.getElementById('pageholder-browser').webProgress.removeProgressListener(panelProgressListener);
}

function loadPlaceholderPage() {
    var panelBrowser = document.getElementById('pageholder-browser');
    var addButton = document.getElementById('addpanel-button');
    panelBrowser.setAttribute("src", "chrome://browser/content/page-drawer.xml");
    addButton.disabled = true;
}

function grabPage()
{
    var panelBrowser = document.getElementById('pageholder-browser');
    try {
      panelBrowser.webNavigation.loadURI(window.parent.gBrowser.currentURI.spec, nsIWebNavigation.LOAD_FLAGS_NONE, null, null, null);
      var addButton = document.getElementById('addpanel-button');
      addButton.disabled = false;
    } catch (e) {}
}

function addWebPanel()
{
    window.parent.addBookmarkAs(document.getElementById('pageholder-browser'), true);
}
