
var RDF = Components.classes['component://netscape/rdf/rdf-service'].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);
var pageTagArc = RDF.GetResource("http://home.netscape.com/NC-rdf#PageTag");

function showPage(event) {

  var node = event.target.parentNode.parentNode;
  var uri = node.getAttribute('id');

  var server = node.parentNode.parentNode.parentNode;
  var serveruri = server.getAttribute('id');

  var pageResource = RDF.GetResource(uri);

  // we have to do this extra QI because of a bug in XPConnect
  var accounttree = window.frames["accounttree"];
  var db = accounttree.document.getElementById("accounttree").database;
  var db2 = db.QueryInterface(Components.interfaces.nsIRDFDataSource);

  // convert node->literal
  var newPageNode = db2.GetTarget(pageResource, pageTagArc, true);
  var newPageResource = newPageNode.QueryInterface(Components.interfaces.nsIRDFLiteral);
  var pageURI = newPageResource.Value;
  
  var destFrame = window.frames["prefframe"];

  // this is a quick hack to make this work for now.
  newLocation = pageURI;
  
  //  var lastSlashPos = oldLocation.lastIndexOf('/');
  //  var lastSlashPos=0;
  
  //  if (lastSlashPos >= 0) {
  //    newLocation = oldLocation.slice(0, lastSlashPos+1);
  //    newLocation += pageURI;
  //  }

  // if (oldLocation != newLocation) {
    destFrame.location = newLocation;
  // }
}


// I copied this from commandglue.js. bad!
// -alecf
function ToggleTwisty(treeItem)
{

	var openState = treeItem.getAttribute('open');
	if(openState == 'true')
	{
		treeItem.removeAttribute('open');
	}
	else
	{
		treeItem.setAttribute('open', 'true');
	}
}

