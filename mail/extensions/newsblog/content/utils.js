// XXX Rename this to global.js

// Whether or not to dump debugging messages to the console.
const DEBUG = true;
var debug;
if (DEBUG)
  debug = function(msg) { dump(' -- FZ -- : ' + msg + '\n'); }
else
  debug = function() {}

var rdf =
  Components
    .classes["@mozilla.org/rdf/rdf-service;1"]
      .getService(Components.interfaces.nsIRDFService);

const RDF_NS = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
const RDF_TYPE = rdf.GetResource(RDF_NS + "type");

const RSS_NS = "http://purl.org/rss/1.0/";
const RSS_CHANNEL = rdf.GetResource(RSS_NS + "channel");
const RSS_TITLE = rdf.GetResource(RSS_NS + "title");
const RSS_DESCRIPTION = rdf.GetResource(RSS_NS + "description");
const RSS_ITEMS = rdf.GetResource(RSS_NS + "items");
const RSS_ITEM = rdf.GetResource(RSS_NS + "item");
const RSS_LINK = rdf.GetResource(RSS_NS + "link");

const RSS_CONTENT_NS = "http://purl.org/rss/1.0/modules/content/";
const RSS_CONTENT_ENCODED = rdf.GetResource(RSS_CONTENT_NS + "encoded");

const DC_NS = "http://purl.org/dc/elements/1.1/";
const DC_CREATOR = rdf.GetResource(DC_NS + "creator");
const DC_SUBJECT = rdf.GetResource(DC_NS + "subject");
const DC_DATE = rdf.GetResource(DC_NS + "date");
const DC_TITLE = rdf.GetResource(DC_NS + "title");
const DC_IDENTIFIER = rdf.GetResource(DC_NS + "identifier");

const FZ_NS = "urn:forumzilla:";
const FZ_ROOT = rdf.GetResource(FZ_NS + "root");
const FZ_FEEDS = rdf.GetResource(FZ_NS + "feeds");
const FZ_FEED = rdf.GetResource(FZ_NS + "feed");
const FZ_QUICKMODE = rdf.GetResource(FZ_NS + "quickMode");
const FZ_DESTFOLDER = rdf.GetResource(FZ_NS + "destFolder");

// XXX There's a containerutils in forumzilla.js that this should be merged with.
var containerUtils =
  Components
    .classes["@mozilla.org/rdf/container-utils;1"]
      .getService(Components.interfaces.nsIRDFContainerUtils);

var fileHandler =
  Components
    .classes["@mozilla.org/network/io-service;1"]
      .getService(Components.interfaces.nsIIOService)
        .getProtocolHandler("file")
          .QueryInterface(Components.interfaces.nsIFileProtocolHandler);

function addFeed(url, title, quickMode, destFolder) {
    var ds = getSubscriptionsDS();
    var feeds = getSubscriptionsList();

    // Give quickMode a default value of "true"; otherwise convert value
    // to either "true" or "false" string.
    quickMode = quickMode == null ? "false" : quickMode ? "true" : "false";

    // Generate a unique ID for the feed.
    var id = url;
    var i = 1;
    while (feeds.IndexOf(rdf.GetResource(id)) != -1 && ++i < 1000)
        id = url + i;
    if (id == 1000)
        throw("couldn't generate a unique ID for feed " + url);

    // Add the feed to the list.
    id = rdf.GetResource(id);
    feeds.AppendElement(id);
    ds.Assert(id, RDF_TYPE, FZ_FEED, true);
    ds.Assert(id, DC_IDENTIFIER, rdf.GetLiteral(url), true);
    if (title)
        ds.Assert(id, DC_TITLE, rdf.GetLiteral(title), true);
    ds.Assert(id, FZ_QUICKMODE, rdf.GetLiteral(quickMode), true);
		ds.Assert(id, FZ_DESTFOLDER, destFolder, true);
    ds = ds.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
    ds.Flush();
}


function getNodeValue(node) {
  if (node && node.textContent)
    return node.textContent;
  else if (node && node.firstChild) {
    var ret = "";
    for (var child = node.firstChild; child; child = child.nextSibling) {
      var value = getNodeValue(child);
      if (value)
        ret += value;
    }

    if (ret)
      return ret;
  }

  return null;
}

function getRDFTargetValue(ds, source, property) {
  var node = ds.GetTarget(source, property, true);
  if (node) {
    node = node.QueryInterface(Components.interfaces.nsIRDFLiteral);
    if (node)
      return node.Value;
  }
  return null;
}

var gFzSubscriptionsDS; // cache
function getSubscriptionsDS() {
    if (gFzSubscriptionsDS)
        return gFzSubscriptionsDS;

    var file = getSubscriptionsFile();
    var url = fileHandler.getURLSpecFromFile(file);

    gFzSubscriptionsDS = rdf.GetDataSource(url);
    if (!gFzSubscriptionsDS)
        throw("can't get subscriptions data source");

    // Note that it this point the datasource may not be loaded yet.
    // You have to QueryInterface it to nsIRDFRemoteDataSource and check
    // its "loaded" property to be sure.  You can also attach an observer
    // which will get notified when the load is complete.

    return gFzSubscriptionsDS;
}

function getSubscriptionsList() {
    var ds = getSubscriptionsDS();
    var list = ds.GetTarget(FZ_ROOT, FZ_FEEDS, true);
    //list = feeds.QueryInterface(Components.interfaces.nsIRDFContainer);
    list = list.QueryInterface(Components.interfaces.nsIRDFResource);
    list = containerUtils.MakeSeq(ds, list);
    return list;
}

function getSubscriptionsFile() {
  // Get the app directory service so we can look up the user's profile dir.
  var appDirectoryService =
    Components
      .classes["@mozilla.org/file/directory_service;1"]
        .getService(Components.interfaces.nsIProperties);
  if ( !appDirectoryService )
    throw("couldn't get the directory service");

  // Get the user's profile directory.
  var profileDir =
    appDirectoryService.get("ProfD", Components.interfaces.nsIFile);
  if ( !profileDir )
    throw ("couldn't get the user's profile directory");

  // Get the user's subscriptions file.
  var file = profileDir.clone();
  file.append("feeds.rdf");

  // If the file doesn't exist, create it.
  if (!file.exists())
    createSubscriptionsFile(file);

  return file;
}

function createSubscriptionsFile(file) {
  file = new LocalFile(file, MODE_WRONLY | MODE_CREATE);
  file.write('\
<?xml version="1.0"?>\n\
<RDF:RDF xmlns:dc="http://purl.org/dc/elements/1.1/"\n\
         xmlns:fz="' + FZ_NS + '"\n\
         xmlns:RDF="http://www.w3.org/1999/02/22-rdf-syntax-ns#">\n\
  <RDF:Description about="' + FZ_ROOT.Value + '">\n\
    <fz:feeds>\n\
      <RDF:Seq>\n\
      </RDF:Seq>\n\
    </fz:feeds>\n\
  </RDF:Description>\n\
</RDF:RDF>\n\
');
  file.close();
}
