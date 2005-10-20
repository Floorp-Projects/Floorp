/* -*- Mode: Java; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
var gPrefInt = null;
var gCurrentDirectory = null;
var gCurrentDirectoryString = null;

const kDefaultMaxHits = 100;
const kDefaultLDAPPort = 389;
const kDefaultSecureLDAPPort = 636;
const kLDAPDirectory = 0;  // defined in nsDirPrefs.h

function Startup()
{
  if ( "arguments" in window && window.arguments[0] ) {
    gCurrentDirectory = window.arguments[0].selectedDirectory;
    gCurrentDirectoryString = window.arguments[0].selectedDirectoryString;
    try {
      fillSettings();
    } catch (ex) {
      dump("pref-directory-add.js:Startup(): fillSettings() exception: " 
           + ex + "\n");
    }
  } else {
    fillDefaultSettings();
  }
}

function DownloadNow()
{
  var args = {dirName: gCurrentDirectory, prefName: gCurrentDirectoryString};

  window.opener.openDialog("chrome://messenger/content/addressbook/replicationProgress.xul", "", "chrome,resizable,status,centerscreen,dialog=no", args);

}

// fill the settings panel with the data from the preferences. 
//
function fillSettings()
{
  gPrefInt = Components.classes["@mozilla.org/preferences-service;1"]
    .getService(Components.interfaces.nsIPrefBranch);

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
  document.getElementById("downloadDisabledMsg").hidden = false;
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
