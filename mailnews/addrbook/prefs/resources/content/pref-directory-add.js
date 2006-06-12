/* -*- Mode: Java; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
var gPrefInt = null;
var gCurrentDirectory = null;
var gCurrentDirectoryString = null;
var gReplicationBundle = null;
var gReplicationService =
  Components.classes["@mozilla.org/addressbook/ldap-replication-service;1"].
             getService(Components.interfaces.nsIAbLDAPReplicationService);
var gReplicationCancelled = false;
var gProgressText;
var gProgressMeter;
var gDownloadInProgress = false;

const kDefaultMaxHits = 100;
const kDefaultLDAPPort = 389;
const kDefaultSecureLDAPPort = 636;
const kLDAPDirectory = 0;  // defined in nsDirPrefs.h

var ldapOfflineObserver = {
  observe: function(subject, topic, state)
  {
    // sanity checks
    if (topic != "network:offline-status-changed") return;
    setDownloadOfflineOnlineState(state == "offline");
  }
}

function Startup()
{
  gPrefInt = Components.classes["@mozilla.org/preferences-service;1"]
    .getService(Components.interfaces.nsIPrefBranch);
  gReplicationBundle = document.getElementById("bundle_replication");

  document.getElementById("download").label =
    gReplicationBundle.getString("downloadButton");
  document.getElementById("download").accessKey =
    gReplicationBundle.getString("downloadButtonAccessKey");

  if ( "arguments" in window && window.arguments[0] ) {
    gCurrentDirectory = window.arguments[0].selectedDirectory;
    gCurrentDirectoryString = window.arguments[0].selectedDirectoryString;
    try {
      fillSettings();
    } catch (ex) {
      dump("pref-directory-add.js:Startup(): fillSettings() exception: " 
           + ex + "\n");
    }

    // Only set up the download button for online/offline status toggling
    // if the pref isn't locked to disable the button.
    if (!gPrefInt.prefIsLocked(gCurrentDirectoryString + ".disable_button_download")) {
      // Now connect to the offline/online observer
      var observerService = Components.classes["@mozilla.org/observer-service;1"]
                                      .getService(Components.interfaces.nsIObserverService);
      observerService.addObserver(ldapOfflineObserver,
                                  "network:offline-status-changed", false);

      // Now set the initial offline/online state.
      var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                                .getService(Components.interfaces.nsIIOService);
      // And update the state
      setDownloadOfflineOnlineState(ioService.offline);
    }
  } else {
    fillDefaultSettings();
    // Don't add observer here as it doesn't make any sense.
  }
}

function onUnload()
{
  if ("arguments" in window && 
      window.arguments[0] &&
      !gPrefInt.prefIsLocked(gCurrentDirectoryString + ".disable_button_download")) {
    // Remove the observer that we put in on dialog startup
    var observerService = Components.classes["@mozilla.org/observer-service;1"]
                                    .getService(Components.interfaces.nsIObserverService);
    observerService.removeObserver(ldapOfflineObserver,
                                   "network:offline-status-changed");
  }
}

var progressListener = {
  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus)
  {
    if (aStateFlags & Components.interfaces.nsIWebProgressListener.STATE_START) {
      // start the spinning
      gProgressMeter.setAttribute("mode", "undetermined");
      gProgressText.value = gReplicationBundle.getString(aStatus ?
                                                         "replicationStarted" :
                                                         "changesStarted");
      gDownloadInProgress = true;
      document.getElementById("download").label =
        gReplicationBundle.getString("cancelDownloadButton");
      document.getElementById("download").accessKey =
        gReplicationBundle.getString("cancelDownloadButtonAccessKey");
    }
    
    if (aStateFlags & Components.interfaces.nsIWebProgressListener.STATE_STOP) {
      EndDownload(aStatus);
    }
  },
  onProgressChange: function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress)
  {
    gProgressText.value = gReplicationBundle.getFormattedString("currentCount",
                                                                [aCurSelfProgress]);
  },
  onLocationChange: function(aWebProgress, aRequest, aLocation)
  {
  },
  onStatusChange: function(aWebProgress, aRequest, aStatus, aMessage)
  {
  },
  onSecurityChange: function(aWebProgress, aRequest, state)
  {
  },
  QueryInterface : function(iid)
  {
    if (iid.equals(Components.interfaces.nsIWebProgressListener) || 
        iid.equals(Components.interfaces.nsISupportsWeakReference) || 
        iid.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_NOINTERFACE;
  }
};

function DownloadNow()
{
  if (!gDownloadInProgress) {
    gProgressText = document.getElementById("replicationProgressText");
    gProgressMeter = document.getElementById("replicationProgressMeter");

    gProgressText.hidden = false;
    gProgressMeter.hidden = false;
    gReplicationCancelled = false;

    try {
      gReplicationService.startReplication(gCurrentDirectoryString,
                                           progressListener);
    }
    catch (ex) {
      EndDownload(false);
    }
  } else {
    gReplicationCancelled = true;
    try {
      gReplicationService.cancelReplication(gCurrentDirectoryString);
    }
    catch (ex) {
      // XXX todo
      // perhaps replication hasn't started yet?  This can happen if you hit cancel after attempting to replication when offline 
      dump("unexpected failure while cancelling.  ex=" + ex + "\n");
    }
  }
}

function EndDownload(aStatus)
{
  document.getElementById("download").label =
    gReplicationBundle.getString("downloadButton");
  document.getElementById("download").accessKey =
    gReplicationBundle.getString("downloadButtonAccessKey");

  // stop the spinning
  gProgressMeter.setAttribute("mode", "normal");
  gProgressMeter.setAttribute("value", "100");
  gProgressMeter.hidden = true;

  gDownloadInProgress = false;
  gProgressText.value =
    gReplicationBundle.getString(aStatus ? "replicationSucceeded" :
                                 gReplicationCancelled ? "replicationCancelled" :
                                  "replicationFailed");
}

// fill the settings panel with the data from the preferences. 
//
function fillSettings()
{
  var ldapUrl = Components.classes["@mozilla.org/network/ldap-url;1"];
  ldapUrl = ldapUrl.createInstance().
    QueryInterface(Components.interfaces.nsILDAPURL);

  try {
    var prefValue = 
      gPrefInt.getComplexValue(gCurrentDirectoryString + ".description",
                               Components.interfaces.nsISupportsString).data;
  } catch(ex) {
    prefValue="";
  }
  
  document.getElementById("description").value = prefValue;
  ldapUrl.spec = gPrefInt.getComplexValue(gCurrentDirectoryString +".uri",
                                          Components.interfaces.
                                          nsISupportsString).data;

  document.getElementById("hostname").value = ldapUrl.host;
  document.getElementById("port").value = ldapUrl.port;

  document.getElementById("basedn").value = ldapUrl.dn;
  document.getElementById("search").value = ldapUrl.filter;

  var sub = document.getElementById("sub");
  switch(ldapUrl.scope) {
  case Components.interfaces.nsILDAPURL.SCOPE_ONELEVEL:
    sub.radioGroup.selectedItem = document.getElementById("one");
    break;
  default:
    sub.radioGroup.selectedItem = sub;
    break;
  }
    
  if (ldapUrl.options & ldapUrl.OPT_SECURE) {
    document.getElementById("secure").setAttribute("checked", "true");
  }

  try {
    prefValue = gPrefInt.getIntPref(gCurrentDirectoryString + ".maxHits");
  } catch(ex) {
    prefValue = kDefaultMaxHits;
  }
  document.getElementById("results").value = prefValue;

  try {
    prefValue = 
      gPrefInt.getComplexValue(gCurrentDirectoryString + ".auth.dn",
                               Components.interfaces.nsISupportsString).data;
  } catch(ex) {
    prefValue="";
  }
  document.getElementById("login").value = prefValue;

  // check if any of the preferences for this server are locked.
  //If they are locked disable them
  DisableUriFields(gCurrentDirectoryString + ".uri");
  DisableElementIfPrefIsLocked(gCurrentDirectoryString + ".description", "description");
  DisableElementIfPrefIsLocked(gCurrentDirectoryString + ".disable_button_download", "download");
  DisableElementIfPrefIsLocked(gCurrentDirectoryString + ".maxHits", "results");
  DisableElementIfPrefIsLocked(gCurrentDirectoryString + ".auth.dn", "login");
}

function DisableElementIfPrefIsLocked(aPrefName, aElementId)
{
  if (gPrefInt.prefIsLocked(aPrefName))
    document.getElementById(aElementId).setAttribute('disabled', true);
}

// disables all the text fields corresponding to the .uri pref.
function DisableUriFields(aPrefName)
{
  if (gPrefInt.prefIsLocked(aPrefName)) {
    var lockedElements = document.getElementsByAttribute("disableiflocked", "true");
    for (var i=0; i<lockedElements.length; i++)
      lockedElements[i].setAttribute('disabled', 'true');
  }
}

function onSecure()
{
  var port = document.getElementById("port");
  if (document.getElementById("secure").checked)
      port.value = kDefaultSecureLDAPPort;
  else
    port.value = kDefaultLDAPPort;
}

function fillDefaultSettings()
{
  document.getElementById("port").value = kDefaultLDAPPort;
  document.getElementById("results").value = kDefaultMaxHits;
  var sub = document.getElementById("sub");
  sub.radioGroup.selectedItem = sub;

  // Disable the download button and add some text indicating why.
  document.getElementById("download").disabled = true;
  document.getElementById("downloadWarningMsg").hidden = false;
  document.getElementById("downloadWarningMsg").textContent = document.
                                      getElementById("bundle_addressBook").
                                      getString("abReplicationSaveSettings");
}

function hasOnlyWhitespaces(string)
{
  // get all the whitespace characters of string and assign them to str.
  // string is not modified in this function
  // returns true if string contains only whitespaces and/or tabs
  var re = /[ \s]/g;
  var str = string.match(re);
  if (str && (str.length == string.length))
    return true;
  else
    return false;
}

function hasCharacters(number)
{
  var re = /[0-9]/g;
  var num = number.match(re);
  if(num && (num.length == number.length))
    return false;
  else
    return true;
}

function onAccept()
{
  var properties = Components.classes["@mozilla.org/addressbook/properties;1"].createInstance(Components.interfaces.nsIAbDirectoryProperties);
  var addressbook = Components.classes["@mozilla.org/addressbook;1"].createInstance(Components.interfaces.nsIAddressBook);
  properties.dirType = kLDAPDirectory;

  try {
    var pref_string_content = "";
    var pref_string_title = "";

    var ldapUrl = Components.classes["@mozilla.org/network/ldap-url;1"];
    ldapUrl = ldapUrl.createInstance().
      QueryInterface(Components.interfaces.nsILDAPURL);

    var description = document.getElementById("description").value;
    var hostname = document.getElementById("hostname").value;
    var port = document.getElementById("port").value;
    var secure = document.getElementById("secure");
    var dn = document.getElementById("login").value;
    var results = document.getElementById("results").value;
    var errorValue = null;
    if ((!description) || hasOnlyWhitespaces(description))
      errorValue = "invalidName";
    else if ((!hostname) || hasOnlyWhitespaces(hostname))
      errorValue = "invalidHostname";
    // XXX write isValidDn and call it on the dn string here?
    else if (port && hasCharacters(port))
      errorValue = "invalidPortNumber";
    else if (results && hasCharacters(results))
      errorValue = "invalidResults";
    if (!errorValue) {
      properties.description = description;
  
      ldapUrl.host = hostname;
      ldapUrl.dn = document.getElementById("basedn").value;
      ldapUrl.filter = document.getElementById("search").value;
      if (!port) {
        if (secure.checked)
          ldapUrl.port = kDefaultSecureLDAPPort;
        else
          ldapUrl.port = kDefaultLDAPPort;
      } else {
        ldapUrl.port = port;
      }
      if (document.getElementById("one").selected) {
        ldapUrl.scope = Components.interfaces.nsILDAPURL.SCOPE_ONELEVEL;
      } else {
        ldapUrl.scope = Components.interfaces.nsILDAPURL.SCOPE_SUBTREE;
      }
      if (secure.checked)
        ldapUrl.options |= ldapUrl.OPT_SECURE;

      properties.URI = ldapUrl.spec;
      properties.maxHits = results;
      properties.authDn = dn;

      // check if we are modifying an existing directory or adding a new directory
      if (gCurrentDirectory && gCurrentDirectoryString) {
        // we are modifying an existing directory
        // the rdf service
        var RDF = Components.classes["@mozilla.org/rdf/rdf-service;1"].
                         getService(Components.interfaces.nsIRDFService);
        // get the datasource for the addressdirectory
        var addressbookDS = RDF.GetDataSource("rdf:addressdirectory");

        // moz-abdirectory:// is the RDF root to get all types of addressbooks.
        var parentDir = RDF.GetResource("moz-abdirectory://").QueryInterface(Components.interfaces.nsIAbDirectory);

        // the RDF resource URI for LDAPDirectory will be moz-abldapdirectory://<prefName>
        var selectedABURI = "moz-abldapdirectory://" + gCurrentDirectoryString;
        var selectedABDirectory = RDF.GetResource(selectedABURI).QueryInterface(Components.interfaces.nsIAbDirectory);
 
        // Carry over existing palm category id and mod time if it was synced before.
        var oldProperties = selectedABDirectory.directoryProperties;
        properties.categoryId = oldProperties.categoryId;
        properties.syncTimeStamp = oldProperties.syncTimeStamp;

        // Now do the modification.
        addressbook.modifyAddressBook(addressbookDS, parentDir, selectedABDirectory, properties);
        window.opener.gNewServerString = gCurrentDirectoryString;       
      }
      else { // adding a new directory
        addressbook.newAddressBook(properties);
        window.opener.gNewServerString = properties.prefName;
      }

      window.opener.gNewServer = description;
      // set window.opener.gUpdate to true so that LDAP Directory Servers
      // dialog gets updated
      window.opener.gUpdate = true; 
    } else {
      var addressBookBundle = document.getElementById("bundle_addressBook");

      var promptService = Components.
                          classes["@mozilla.org/embedcomp/prompt-service;1"].
                          getService(Components.interfaces.nsIPromptService);

      promptService.alert(window,
                          document.title,
                          addressBookBundle.getString(errorValue));
      return false;
    }
  } catch (outer) {
    dump("Internal error in pref-directory-add.js:onAccept() " + outer + "\n");
  }
  return true;
}

function onCancel()
{  
  window.opener.gUpdate = false;
}


// called by Help button in platform overlay
function doHelpButton()
{
  openHelp("mail-ldap-properties");
}

// Sets the download button state for offline or online.
// This function should only be called for ldap edit dialogs.
function setDownloadOfflineOnlineState(isOffline)
{
  if (isOffline)
  {
    // Disable the download button and add some text indicating why.
    document.getElementById("downloadWarningMsg").textContent = document.
      getElementById("bundle_addressBook").
      getString("abReplicationOfflineWarning");
  }
  document.getElementById("downloadWarningMsg").hidden = !isOffline;
  document.getElementById("download").disabled = isOffline;
}
