var gSpamSettings;
varg gServer;

function onJunkMailLoad()
{
  if (window.arguments && window.arguments[0])
    setupForAccountFromFolder(window.arguments[0].folder);
}

function onServerClick()
{
}

function setupForAccountFromFolder(folder)
{
  // make sure we do the right thing for the stand alone msg window
  try {
    var msgFolder = GetMsgFolderFromUri(folder.URI, false);
    gServer = msgFolder.server;
  }
  catch (ex) {
    // get server for default account
    var accountManager = Components.classes["@mozilla.org/messenger/account-manager;1"]
               .getService(Components.interfaces.nsIMsgAccountManager);
    var account = accountManager.defaultAccount;
    gServer = account.incomingServer;
  }

  // get and clone spam settings for this server
  // we clone because if the users cancels we are going to throw away the changes
  gSpamSettings = Components.classes["@mozilla.org/messenger/spamsettings;1"].createInstance(Components.interfaces.nsISpamSettings);
  gSpamSettings.clone(gServer.spamSettings);

  // select server in the menulist
  var serverList = document.getElementById("server");
  var menuitems = serverList.getElementsByAttribute("id", server.serverURI);
  serverList.selectedItem = menuitems[0];

  // set up the UI for this server
  document.getElementById("level").value = gSpamSettings.level;

  document.getElementById("moveOnSpam").checked = gSpamSettings.moveOnSpam;
  // todo, use  attribute string actionTargetFolder;

  document.getElementById("purge").checked = gSpamSettings.purge;
  document.getElementById("purgeInterval").value = gSpamSettings.purgeInterval;

  document.getElementById("useWhiteList").checked = gSpamSettings.useWhiteList;
  var abList = document.getElementById("whiteListAbURI");
  menuitems = abList.getElementsByAttribute("id", gSpamSettings.whiteListAbURI);
  abList.selectedItem = menuitems[0];
}

function junkLog()
{
  // pass in the "real" spam settings, as it's the one with the logStream
  var args = {spamSettings: gServer.spamSettings};
  window.openDialog("chrome://messenger/content/junkLog.xul", "junkLog", "chrome,modal,titlebar,resizable,centerscreen", args);
}

function onAccept()
{
  // if they hit ok, set the "real" server's spam settings.  this will set prefs.
  gServer.spamSettings = gSpamSettings;
}

function doHelpButton()
{
}