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
            document.getElementById('webpanels-throbber').setAttribute("loading", "true");
        }
        else if (aStateFlags & nsIWebProgressListener.STATE_STOP &&
                aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK) {
            document.getElementById('webpanels-throbber').removeAttribute("loading");
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
        document.getElementById('webpanels-browser').webNavigation.loadURI(uri, nsIWebNavigation.LOAD_FLAGS_NONE, null, null, null);
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

function loadWebPanel(aURI) {
    gLoadPlaceHolder = false;
    try {
      document.getElementById('webpanels-browser').webNavigation.loadURI(aURI, nsIWebNavigation.LOAD_FLAGS_NONE, null, null, null);
      var addButton = document.getElementById('addpanel-button');
      addButton.disabled = true;
    } catch (e) {}
}


function load()
{
  document.getElementById('webpanels-browser').webProgress.addProgressListener(panelProgressListener, Components.interfaces.nsIWebProgress.NOTIFY_ALL);
  loadPlaceholderPage();
}

function unload()
{
  document.getElementById('webpanels-browser').webProgress.removeProgressListener(panelProgressListener);
}

// We do this in the onload in order to make sure that the load of this page doesn't delay the onload of
// the sidebar itself.
var gLoadPlaceHolder = true;
function loadPlaceholderPage() {
    var panelBrowser = document.getElementById('webpanels-browser');
    var addButton = document.getElementById('addpanel-button');
    if (gLoadPlaceHolder) {
        panelBrowser.setAttribute("src", "chrome://browser/content/web-panels.xml");
        addButton.disabled = true;
    }
}


function grabPage()
{
    var panelBrowser = document.getElementById('webpanels-browser');
    try {
      panelBrowser.webNavigation.loadURI(window.parent.gBrowser.currentURI.spec, nsIWebNavigation.LOAD_FLAGS_NONE, null, null, null);
      var addButton = document.getElementById('addpanel-button');
      addButton.disabled = false;
    } catch (e) {}
}

function addWebPanel()
{
    window.parent.addBookmarkAs(document.getElementById('webpanels-browser'), true);
}
