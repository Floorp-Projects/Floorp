// XXX MAKE SURE ENSURANCE_DELAY AND DEBUG ARE CORRECT BEFORE SHIPPING!!!

// How long to wait after accessing all the message databases before we start
// downloading feeds and items.  The goal is to wait the minimum length of time
// to ensure the databases are available when we check to see if items are
// already downloaded.  This works around the problem in which the databases
// claim not to know about messages that have already been downloaded if we ask
// them too soon after retrieving a reference to them, resulting in duplicate
// messages as we redownload items thinking they are new.
//const ENSURANCE_DELAY = 2000; // XXX FOR TESTING ONLY
const ENSURANCE_DELAY = 15000;

// The name of the local mail server in which Forumzilla creates feed folders.
// XXX Make this configurable.
const SERVER_NAME = "News & Blogs";

// Number of items currently being loaded.  gFzIncomingServer.serverBusy will be
// true while this number is greater than zero.
//var gFzMessagesBeingLoaded = 0;

var gFzPrefs =
  Components
    .classes["@mozilla.org/preferences-service;1"]
      .getService(Components.interfaces.nsIPrefService)
        .getBranch("forumzilla.");

var gFzStartupDelay;
try      { gFzStartupDelay = gFzPrefs.getIntPref("startup.delay") }
catch(e) { gFzStartupDelay = 2; }

// Record when we started up so we can give up trying certain things
// repetitively after some time.
var gFzStartupTime = new Date();

// Load and cache the subscriptions data source so it's available when we need it.
getSubscriptionsDS();

function onLoad() {
  // XXX Code to make the News & Blogs toolbar button show up automatically.
  // Commented out because it isn't working quite right.
  //var toolbar = document.getElementById('mail-bar');
  //var currentset = toolbar.getAttribute('currentset');
  //if (!currentset.search("button-newsandblogs") == -1)
  //  toolbar.insertItem('button-newsandblogs', 'button-stop', null, true);
  //  currentset = currentset.replace(/spring/, "button-newsandblogs,spring");
  //if (!currentset.search("button-newsandblogs") == -1)
  //  currentset += ",button-newsandblogs";
  //toolbar.setAttribute('currentset', currentset);

  // Make sure the subscriptions data source is loaded, since we'll need it
  // for everything we do.  If it's not loaded, recheck it every second until
  // it's been ten seconds since we started up, then give up.
  var ds = getSubscriptionsDS();
  var remote = ds.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
  if (!remote.loaded) {
    if (new Date() - gFzStartupTime < 10 * 1000)
      window.setTimeout(onLoad, 1000);
    else
      throw("couldn't load the subscriptions datasource within ten seconds");
    return;
  }

  var subs = getSubscriptionsFile();

  var oldSubs = getOldSubscriptionsFile();
  if (oldSubs.exists())
    migrateSubscriptions(oldSubs);

  getAccount();
  ensureDatabasesAreReady();
  downloadFeeds();
}

// Wait a few seconds before starting Forumzilla so we don't grab the UI thread
// before the host application has a chance to display itself.  Also, starting
// on load used to crash the host app, although that's not a problem anymore.
//addEventListener("load", onLoad, false);
window.setTimeout(onLoad, gFzStartupDelay * 1000);

function downloadFeeds() {
  // Reload subscriptions every 30 minutes (XXX make this configurable via a pref).
  window.setTimeout(downloadFeeds, 30 * 60 * 1000);

  var ds = getSubscriptionsDS();
  var feeds = getSubscriptionsList().GetElements();

  var feed, url, quickMode, title;
  while(feeds.hasMoreElements()) {
    feed = feeds.getNext();
    feed = feed.QueryInterface(Components.interfaces.nsIRDFResource);

    url = ds.GetTarget(feed, DC_IDENTIFIER, true);
    if (url)
      url = url.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;

    quickMode = ds.GetTarget(feed, FZ_QUICKMODE, true);
    if (quickMode) {
      quickMode = quickMode.QueryInterface(Components.interfaces.nsIRDFLiteral);
      quickMode = quickMode.Value;
      quickMode = eval(quickMode);
    }

    title = ds.GetTarget(feed, DC_TITLE, true);
    if (title)
      title = title.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;

    feed = new Feed(url, quickMode, title);
    feed.download();
  }
}

function migrateSubscriptions(oldFile) {
  var oldFile2 = new LocalFile(oldFile, MODE_RDONLY | MODE_CREATE);
  var subscriptions = oldFile2.read();
  if (subscriptions && subscriptions.length > 0) {
    subscriptions = subscriptions.split(/[\r\n]+/);
    var feeds = getSubscriptionsList();

    var url, quickMode;
    for ( var i=0 ; i<subscriptions.length ; i++ ) {
      url = subscriptions[i];
      quickMode = 0;

      // Trim whitespace around the URL.
      url = url.replace(/^\s+/, "");
      url = url.replace(/\s+$/, "");

      // If the URL is prefixed by a dollar sign, the feed is in quick mode,
      // which means it won't download item content but rather construct a message
      // from whatever information is available in the feed file itself
      // (i.e. content, description, etc.).
      if (url[0] == '$') {
        quickMode = 1;
        url = url.substr(1);
        url = url.replace(/^\s+/, "");
      }

      // Ignore blank lines and comments.
      if (url.length == 0 || url[0] == "#")
        continue;
			// ### passing in null for the folder may not work...but we may not care
			// about migrating old subscriptions
      if (feeds.IndexOf(rdf.GetResource(url)) == -1)
        addFeed(url, null, quickMode, null);
    }
  }
  oldFile2.close();
  oldFile.moveTo(null, "subscriptions.txt.bak");
}


function ensureDatabasesAreReady() {
  debug("ensuring databases are ready for feeds");

  getIncomingServer();

  var folder;
  var db;

  var folders = gFzIncomingServer.rootMsgFolder.GetSubFolders();
  var done = false;

  while (!done) {
    try {
      folder = folders.currentItem();
      folder = folder.QueryInterface(Components.interfaces.nsIMsgFolder);
      debug("ensuring database is ready for feed " + folder.name);
      try {
        db = folder.getMsgDatabase(msgWindow);
      } catch(e) {
        debug("error getting database: " + e);
      }
      if (!db) {
        debug("couldn't get database");
      }
      else {
        debug("got database " + db);
      }
      folders.next();
    }
    catch (e) {
      done = true;
    }
  }

  debug("ensurance completed; databases should be ready for feeds");
}

function getOldSubscriptionsFile() {
  // Get the app directory service so we can look up the user's profile dir.
  var appDirectoryService =
    Components
      .classes["@mozilla.org/file/directory_service;1"]
        .getService(Components.interfaces.nsIProperties);
  if ( !appDirectoryService )
    throw("couldn't retrieve the directory service");

  // Get the user's profile directory.
  var profileDir =
    appDirectoryService.get("ProfD", Components.interfaces.nsIFile);
  if ( !profileDir )
    throw ("couldn't retrieve the user's profile directory");

  // Get the user's subscriptions file.
  var subscriptionsFile = profileDir.clone();
  subscriptionsFile.append("subscriptions.txt");

  return subscriptionsFile;
}

var gFzIncomingServer; // cache
function getIncomingServer() {

  if (gFzIncomingServer)
    return gFzIncomingServer;

  gFzIncomingServer = accountManager.FindServer("nobody", SERVER_NAME, "rss");

  return gFzIncomingServer;
}

function getMessageWindow() {
  return msgWindow;
}

var gFzAccount; // cache
function getAccount() {
  if (gFzAccount)
    return gFzAccount;

	try {
		gFzAccount = accountManager.FindAccountForServer(getIncomingServer());
	}
	catch (ex) {
		debug("no incoming server or account; creating account...");
		gFzAccount = createAccount();
	}
	return gFzAccount;
}

function createAccount() {
  // I don't think we need an identity, at least not yet.  If we did, though,
  // this is how we would create it, and then we'd use the commented-out
  // addIdentity() call below to add it to the account.
  //var identity = accountManager.createIdentity();
  //identity.email="<INSERT IDENTITY HERE>";

  var server = accountManager.createIncomingServer("nobody", SERVER_NAME, "rss");

  // XXX What's the difference between "name" and "prettyName"?
  // This seems to set the name, not the pretty name, but it does what I want,
  // which is to display this name in the folder pane of the mail window.
  server.prettyName = SERVER_NAME;

  var account = accountManager.createAccount();
  if (!account)
    throw("couldn't create account");

  account.incomingServer = server;
  //account.addIdentity(identity);

  return account;
}

/*
  function updateServerBusyState(numMessagesLoaded) {
  // Not sure if we need this.  nsMovemailService.cpp does it, and it might
  // fix the problem where the user opens the folder while messages
  // are being downloaded and the folder stops updating until the application
  // is restarted.  Because we load messages asynchronously, we have to
  // count the number of messages we're loading so we can set serverBusy
  // to false when all messages have been loaded.
  gFzMessagesBeingLoaded += numMessagesLoaded;
  getIncomingServer();
  if (!gFzIncomingServer.serverBusy && gFzMessagesBeingLoaded > 0) {
    gFzIncomingServer.serverBusy = true;
    debug("marking Feeds server as being busy");
  }
  else if (gFzIncomingServer.serverBusy && gFzMessagesBeingLoaded <= 0) {
    gFzIncomingServer.serverBusy = false;
    debug("marking Feeds server as no longer being busy");
  }
}
*/
