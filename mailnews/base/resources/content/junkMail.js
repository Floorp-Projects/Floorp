var gSpamSettings = {};
var gCurrentServer;
var gMessengerBundle;

function onJunkMailLoad()
{
  gMessengerBundle = document.getElementById("bundle_messenger");
  if (window.arguments && window.arguments[0]) {
    // XXX todo, what if no folder?
    setupForAccountFromFolder(window.arguments[0].folder.URI);
  }
}

function onServerClick(event)
{ 
  if (gCurrentServer.serverURI == event.target.id)
    return;

  // before we set the UI for the new server,
  // save off the old one
  storeSettings(gSpamSettings[gCurrentServer.key].settings, gCurrentServer.spamSettings.loggingEnabled);

  // set up the UI for the server
  setupForAccountFromFolder(event.target.id);
}

function setupForAccountFromFolder(aURI)
{
  // XXX todo
  // XXX make sure we do the right thing for the stand alone msg window
  // XXX if no folders are selected or if a folder that doesn't get mail (news, local?) is selected
  try {
    var msgFolder = GetMsgFolderFromUri(aURI, false);
    gCurrentServer = msgFolder.server;
  }
  catch (ex) {
    // get server for default account
    var accountManager = Components.classes["@mozilla.org/messenger/account-manager;1"]
               .getService(Components.interfaces.nsIMsgAccountManager);
    var account = accountManager.defaultAccount;
    gCurrentServer = account.incomingServer;
  }

  var obj;
  var key = gCurrentServer.key;

  if (key in gSpamSettings) {
    obj = gSpamSettings[key];
  }
  else {
    // get and clone spam settings for this server
    // we clone because if the users cancels we are going to throw away the changes
    var settings = Components.classes["@mozilla.org/messenger/spamsettings;1"].createInstance(Components.interfaces.nsISpamSettings);
    settings.clone(gCurrentServer.spamSettings);
    obj = {server: gCurrentServer, settings: settings}; 
    gSpamSettings[key] = obj;
  } 

  // select server in the menulist
  var serverList = document.getElementById("server");
  var menuitems = serverList.getElementsByAttribute("id", obj.server.serverURI);
  serverList.selectedItem = menuitems[0];

  // set up the UI for this server
  // set up the level checkbox
  document.getElementById("level").checked = (obj.settings.level > 0);

  // set up the junk mail folder picker
  document.getElementById("moveOnSpam").checked = obj.settings.moveOnSpam;
  document.getElementById("moveTargetMode").selectedItem = document.getElementById("moveTargetMode" + obj.settings.moveTargetMode);

  // if there is a target account, use it.  else use the current account
  SetFolderPicker(obj.settings.actionTargetAccount ? obj.settings.actionTargetAccount : obj.server.serverURI, "actionTargetAccount");
  
  if (obj.settings.actionTargetFolder)
    SetFolderPicker(obj.settings.actionTargetFolder, "actionTargetFolder");

  // set up the purge UI
  document.getElementById("purge").checked = obj.settings.purge;
  document.getElementById("purgeInterval").value = obj.settings.purgeInterval;

  // set up the whitelist UI
  document.getElementById("useWhiteList").checked = obj.settings.useWhiteList;
  var abList = document.getElementById("whiteListAbURI");
  menuitems = abList.getElementsByAttribute("id", obj.settings.whiteListAbURI);
  abList.selectedItem = menuitems[0];

  conditionallyEnableUI(null);
}

function junkLog()
{
  // pass in the "real" spam settings, as it's the one with the logStream
  var args = {spamSettings: gCurrentServer.spamSettings};
  window.openDialog("chrome://messenger/content/junkLog.xul", "junkLog", "chrome,modal,titlebar,resizable,centerscreen", args);
}

function onAccept()
{
  // store the current changes
  storeSettings(gSpamSettings[gCurrentServer.key].settings, gCurrentServer.spamSettings.loggingEnabled);

  for (var key in gSpamSettings) {
    // if they hit ok, set the "real" server's spam settings.  
    // this will set prefs.
    gSpamSettings[key].server.spamSettings = gSpamSettings[key].settings;
  }
  return true;
}

function storeSettings(aSettings, aLoggingEnabled)
{
  aSettings.level = document.getElementById("level").checked ? 100 : 0;
  aSettings.moveOnSpam = document.getElementById("moveOnSpam").checked;
  aSettings.moveTargetMode = document.getElementById("moveTargetMode").selectedItem.getAttribute("value");
  aSettings.actionTargetAccount = document.getElementById("actionTargetAccount").getAttribute("uri");
  aSettings.actionTargetFolder = document.getElementById("actionTargetFolder").getAttribute("uri");

  aSettings.purge = document.getElementById("purge").checked;
  aSettings.purgeInterval = document.getElementById("purgeInterval").value;

  aSettings.useWhiteList = document.getElementById("useWhiteList").checked;
  aSettings.whiteListAbURI = document.getElementById("whiteListAbURI").selectedItem.getAttribute("id");
  aSettings.loggingEnabled = aLoggingEnabled;
}

function conditionallyEnableUI(id)
{
  if (!document.getElementById("level").checked) {
    document.getElementById("useWhiteList").disabled = true;
    document.getElementById("whiteListAbURI").disabled = true;
    document.getElementById("moveOnSpam").disabled = true;

    document.getElementById("moveTargetMode0").disabled = true;
    document.getElementById("actionTargetAccount").disabled = true;
    document.getElementById("moveTargetMode1").disabled = true;
    document.getElementById("actionTargetFolder").disabled = true;

    document.getElementById("purge").disabled = true;
    document.getElementById("purgeInterval").disabled = true;
    document.getElementById("purgeLabel").disabled = true;
    return;
  }

  document.getElementById("useWhiteList").disabled = false;

  // document.getElementById("moveOnSpam").disabled = false;
  // temporarily disabling until we get a few more bugs fixed
  //
  document.getElementById("moveOnSpam").disabled = true;

  var enabled;
  if (!id || id == "moveOnSpam") {
    enabled = document.getElementById("moveOnSpam").checked;
    var choice = document.getElementById("moveTargetMode").selectedItem.getAttribute("value");
 
    document.getElementById("moveTargetMode0").disabled = !enabled || (choice == 1);
    document.getElementById("actionTargetAccount").disabled = !enabled || (choice == 1);
    document.getElementById("moveTargetMode1").disabled = !enabled || (choice == 0);
    document.getElementById("actionTargetFolder").disabled = !enabled || (choice == 0);

    var checked = document.getElementById("purge").checked;
    document.getElementById("purge").disabled = !enabled;
    document.getElementById("purgeInterval").disabled = !enabled || !checked;
    document.getElementById("purgeLabel").disabled = !enabled || !checked;
  }

  if (id == "purge") {
    enabled = document.getElementById("purge").checked;
    document.getElementById("purgeInterval").disabled = !enabled;
    document.getElementById("purgeLabel").disabled = !enabled;
  }

  if (!id || id == "useWhiteList") {
    enabled = document.getElementById("useWhiteList").checked;
    document.getElementById("whiteListAbURI").disabled = !enabled;
  }
}

function doHelpButton()
{
  // implement help

  return false; // don't close the top-level window
}
