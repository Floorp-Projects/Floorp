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
    } catch (e) {}
}

// We do this in the onload in order to make sure that the load of this page doesn't delay the onload of
// the sidebar itself.
var gLoadPlaceHolder = true;
function loadPlaceholderPage() {
    var panelBrowser = document.getElementById('webpanels-browser');
    if (gLoadPlaceHolder)
        panelBrowser.setAttribute("src", "chrome://browser/content/web-panels.xml");
}


function grabPage()
{
    var panelBrowser = document.getElementById('webpanels-browser');
    try {
      panelBrowser.webNavigation.loadURI(window.parent.gBrowser.currentURI.spec, nsIWebNavigation.LOAD_FLAGS_NONE, null, null, null);
    } catch (e) {}
}

