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
const FZ_STORED = rdf.GetResource(FZ_NS + "stored");
const FZ_VALID = rdf.GetResource(FZ_NS + "valid");

const RDF_LITERAL_TRUE = rdf.GetLiteral("true");
const RDF_LITERAL_FALSE = rdf.GetLiteral("false");

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
    var ds = getSubscriptionsDS(destFolder.server);
    var feeds = getSubscriptionsList(destFolder.server);

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

// updates the "feedUrl" property in the message database for the folder in question.

var kFeedUrlDelimiter = '|'; // the delimiter used to delimit feed urls in the msg folder database "feedUrl" property

function updateFolderFeedUrl(aFolder, aFeedUrl, aRemoveUrl)
{
  var msgdb = aFolder.QueryInterface(Components.interfaces.nsIMsgFolder).getMsgDatabase(null);
  var folderInfo = msgdb.dBFolderInfo;
  var oldFeedUrl = folderInfo.getCharPtrProperty("feedUrl");

  if (aRemoveUrl)
  { 
    // remove our feed url string from the list of feed urls
    var newFeedUrl = oldFeedUrl.replace(kFeedUrlDelimiter + aFeedUrl, "");
    folderInfo.setCharPtrProperty("feedUrl", newFeedUrl);
  }  
  else 
    folderInfo.setCharPtrProperty("feedUrl", oldFeedUrl + kFeedUrlDelimiter + aFeedUrl);  
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
function getSubscriptionsDS(server) {
    if (gFzSubscriptionsDS)
        return gFzSubscriptionsDS;

    var file = getSubscriptionsFile(server);
    var url = fileHandler.getURLSpecFromFile(file);

    gFzSubscriptionsDS = rdf.GetDataSourceBlocking(url);

    if (!gFzSubscriptionsDS)
        throw("can't get subscriptions data source");

    return gFzSubscriptionsDS;
}

function getSubscriptionsList(server) {
    var ds = getSubscriptionsDS(server);
    var list = ds.GetTarget(FZ_ROOT, FZ_FEEDS, true);
    //list = feeds.QueryInterface(Components.interfaces.nsIRDFContainer);
    list = list.QueryInterface(Components.interfaces.nsIRDFResource);
    list = containerUtils.MakeSeq(ds, list);
    return list;
}

function getSubscriptionsFile(server) {
  server.QueryInterface(Components.interfaces.nsIRssIncomingServer);
  var file = server.subscriptionsDataSourcePath;

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

var gFzItemsDS; // cache
function getItemsDS(server) {
    if (gFzItemsDS)
        return gFzItemsDS;

    var file = getItemsFile(server);
    var url = fileHandler.getURLSpecFromFile(file);

    gFzItemsDS = rdf.GetDataSourceBlocking(url);
    if (!gFzItemsDS)
        throw("can't get subscriptions data source");

    // Note that it this point the datasource may not be loaded yet.
    // You have to QueryInterface it to nsIRDFRemoteDataSource and check
    // its "loaded" property to be sure.  You can also attach an observer
    // which will get notified when the load is complete.

    return gFzItemsDS;
}

function getItemsFile(server) {
  server.QueryInterface(Components.interfaces.nsIRssIncomingServer);
  var file = server.subscriptionsDataSourcePath;

  // If the file doesn't exist, create it.
  if (!file.exists()) {
    var newfile = new LocalFile(file, MODE_WRONLY | MODE_CREATE);
    newfile.write('\
<?xml version="1.0"?>\n\
<RDF:RDF xmlns:dc="http://purl.org/dc/elements/1.1/"\n\
         xmlns:fz="' + FZ_NS + '"\n\
         xmlns:RDF="http://www.w3.org/1999/02/22-rdf-syntax-ns#">\n\
</RDF:RDF>\n\
');
    newfile.close();
  }
  return file;
}

function removeAssertions(ds, resource) {
    var properties = ds.ArcLabelsOut(resource);
    var property;
    while (properties.hasMoreElements()) {
        property = properties.getNext();
        var values = ds.GetTargets(resource, property, true);
        var value;
        while (values.hasMoreElements()) {
            value = values.getNext();
            ds.Unassert(resource, property, value, true);
        }
    }
}
