// Eventually, these DS calls should be removed once we verify that all the consumers 
// try to get the database first. 
// Load and cache the subscriptions data source so it's available when we need it.
getSubscriptionsDS();
getItemsDS();

const SERVER_NAME = "News & Blogs";

var gFzIncomingServer; // cache
function getIncomingServer() {

  if (gFzIncomingServer)
    return gFzIncomingServer;

  var accountManager = Components.classes["@mozilla.org/messenger/account-manager;1"]
                      .getService(Components.interfaces.nsIMsgAccountManager);

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

  var accountManager = Components.classes["@mozilla.org/messenger/account-manager;1"]
                       .getService(Components.interfaces.nsIMsgAccountManager);

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

  var accountManager = Components.classes["@mozilla.org/messenger/account-manager;1"]
                      .getService(Components.interfaces.nsIMsgAccountManager);

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
