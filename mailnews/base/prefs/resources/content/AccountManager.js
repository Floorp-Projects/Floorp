
var RDF = Components.classes['component://netscape/rdf/rdf-service'].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);
var pageTagArc = RDF.GetResource("http://home.netscape.com/NC-rdf#PageTag");

function showPage(event) {

  // the url to load is stored in the PageTag attribute
  // of the current treeitem
  var node = event.target.parentNode.parentNode;

  if (node.tagName != "treeitem") return;
  
  var pageUrl = node.getAttribute('PageTag');

  // the server's URI the parent treeitem
  var servernode = node.parentNode.parentNode;

  // for toplevel treeitems, we just use the current treeitem
  if (servernode.tagName != "treeitem") {
    servernode = node;
  }
  var serveruri = servernode.getAttribute('id');

  // find the IFRAME that corresponds to this server
  var destFrame = top.window.frames[serveruri];

  var newLocation="";
  var oldLocation = new String(destFrame.location);

  // this is a quick hack to make this work for now.
  //  newLocation = pageURI;

  var lastSlashPos = oldLocation.lastIndexOf('/');
  
  if (lastSlashPos >= 0) {
    newLocation = oldLocation.slice(0, lastSlashPos+1);
  }
  newLocation += pageUrl;
  
  if (oldLocation != newLocation) {
    destFrame.location = newLocation;
  }

  // now see if we need to bring this server into view

  // find the box's index in the <deck> and bring it forward
  var deckBox= top.document.getElementById(serveruri);
  var deck = deckBox.parentNode;
  var children = deck.childNodes;
  var i;
  for (i=0; i<children.length; i++) {
    if (children[i] == deckBox) break;
  }

  deck.setAttribute("value", i-1);
}
