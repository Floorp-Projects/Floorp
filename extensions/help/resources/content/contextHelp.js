const MOZ_HELP_URI = "chrome://help/content/help.xul";
const MOZILLA_HELP = "chrome://help/locale/mozillahelp.rdf";
var defaultHelpFile = MOZILLA_HELP;

// Call this function to display a help topic.
// uri: [chrome uri of rdf help file][?topic]
function openHelp(uri) {
  var URI = normalizeURI(uri); // Unpacks helpFileURI and initialTopic;
  var topWindow = locateHelpWindow(URI.helpFileURI);
  if ( topWindow ) {
    topWindow.focus();
    topWindow.displayTopic(URI.topic);
  } else {
    var encodedURI = encodeURIComponent(URI.uri);  
    window.open(MOZ_HELP_URI + "?" + encodedURI, "_blank", "chrome,menubar,toolbar,dialog=no,resizable,scrollbars");
  }
}

function setDefaultHelpFile(rdfURI) {
  defaultHelpFile = rdfURI; 
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

function normalizeURI(uri) {
  // uri in format [uri of help rdf file][?initial topic]
  // if the whole uri or help file uri is ommitted then the default help is assumed.
  // unpack uri
  var URI = {};
  URI.helpFileURI = defaultHelpFile;
  URI.topic = null;
  // Case: No help uri at all.
  if (uri) {
    // Case: Just the search parameter.
    if (uri.substr(0,1) == "?") 
      URI.topic = uri.substr(1);
    else {
      // Case: Full uri with optional topic.
      var i = uri.indexOf("?");
      if ( i != -1) {
        URI.helpFileURI = uri.substr(0,i);
        URI.topic = uri.substr(i+1);
      }
      else 
        URI.helpFileURI = uri;
    }
  }  
  URI.uri = URI.helpFileURI + ((URI.topic)? "?" + URI.topic : "");
  return URI;
}

