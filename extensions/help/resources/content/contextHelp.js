const MOZ_HELP_URI = "chrome://help/content/help.xul";
const MOZILLA_HELP = "chrome://help/locale/mozillahelp.rdf";
var helpFileURI = MOZILLA_HELP;

// Call this function to display a help topic.
// uri: [chrome uri of rdf help file][?topic]
function openHelp(topic) {
  var topWindow = locateHelpWindow(helpFileURI);
  if ( topWindow ) {
    topWindow.focus();
    topWindow.displayTopic(topic);
  } else {
      var encodedURI = encodeURIComponent(helpFileURI + "?" + ((topic)?topic:""));  
      window.open(MOZ_HELP_URI + "?" + encodedURI, "_blank", "chrome,menubar,toolbar,dialog=no,resizable,scrollbars");
  }
}

function setHelpFileURI(rdfURI) {
  helpFileURI = rdfURI; 
}

// Locate mozilla:help window (if any) opened for this help file uri.
function locateHelpWindow(helpFileURI) {
  var windowManager = Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator'].getService();
  var windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
  var iterator = windowManagerInterface.getEnumerator( "mozilla:help");
  var topWindow = null;
  while (iterator.hasMoreElements()) {
    var aWindow = iterator.getNext();
    if (aWindow.getHelpFileURI() == helpFileURI) {
      topWindow = aWindow;
      break;  
    }  
  }
  return topWindow;
}
