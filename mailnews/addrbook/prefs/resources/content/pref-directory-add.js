/* -*- Mode: Java; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
var gPrefstring = "ldap_2.servers.";
var gPref_string_desc = "";
var gPrefInt = null;
var gCurrentDirectory = null;
var gCurrentDirectoryString = null;
var gLdapService = Components.classes["@mozilla.org/network/ldap-service;1"].
    getService(Components.interfaces.nsILDAPService);

const kDefaultMaxHits = 100;
const kDefaultLDAPPort = 389;
const kDefaultSecureLDAPPort = 636;

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

  document.getElementById("basedn").value = 
    gLdapService.UTF8toUCS2(ldapUrl.dn);
  document.getElementById("search").value = 
    gLdapService.UTF8toUCS2(ldapUrl.filter);

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
}

// find a unique server-name for the new directory server
// from the description entered by the user.
function createUniqueServername()
{
  // gPref_string_desc is the description entered by the user.
  // Just get the alphabets and digits from the description.
  // remove spaces and special characters from description.
  var user_Id = 0;
  var re = /[A-Za-z0-9]/g;
  var str = gPref_string_desc.match(re);
  var temp = "";

  if (!str) {
    try {
      user_Id = gPrefInt.getIntPref("ldap_2.user_id");
    }
    catch(ex){ 
      user_Id = 0;
    }
    ++user_Id;
    temp = "user_directory_" + user_Id;
    str = temp;
    try {
      gPrefInt.setIntPref("ldap_2.user_id", user_Id);
    }
    catch(ex) {}    
  }
  else {
    var len = str.length;
    if (len > 20) len = 20;
    for (var count = 0; count < len; count++)
    {
     temp += str[count];
    }
  }

  gPref_string_desc = temp;
  while (temp) {
    temp = "";
    try{ 
      temp = gPrefInt.getComplexValue(gPrefstring+gPref_string_desc+".description",
                                      Components.interfaces.nsISupportsString).data;
    } catch(e){}
    if (temp)
      gPref_string_desc += str[0];
  }
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
  try {
    var pref_string_content = "";
    var pref_string_title = "";

    if (!gPrefInt) {
      gPrefInt = Components.classes["@mozilla.org/preferences-service;1"]
        .getService(Components.interfaces.nsIPrefBranch);
    }

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
    gPref_string_desc = description;
    if ((!gPref_string_desc) || hasOnlyWhitespaces(gPref_string_desc))
      errorValue = "invalidName";
    else if ((!hostname) || hasOnlyWhitespaces(hostname))
      errorValue = "invalidHostname";
    // XXX write isValidDn and call it on the dn string here?
    else if (port && hasCharacters(port))
      errorValue = "invalidPortNumber";
    else if (results && hasCharacters(results))
      errorValue = "invalidResults";
    if (!errorValue) {
      pref_string_content = gPref_string_desc;
      if (gCurrentDirectory && gCurrentDirectoryString) {
        gPref_string_desc = gCurrentDirectoryString;
      } else {
        createUniqueServername();
        gPref_string_desc = gPrefstring + gPref_string_desc;
      }

      pref_string_title = gPref_string_desc + "." + "description";
      var str = Components.classes["@mozilla.org/supports-string;1"]
        .createInstance(Components.interfaces.nsISupportsString);
      str.data = pref_string_content;
      gPrefInt.setComplexValue(pref_string_title, 
                               Components.interfaces.nsISupportsString, str);
  
      ldapUrl.host = hostname;
      pref_string_content = gLdapService.
        UCS2toUTF8(document.getElementById("basedn").value);
      ldapUrl.dn = pref_string_content;
      pref_string_content = gLdapService.
        UCS2toUTF8(document.getElementById("search").value);
      ldapUrl.filter = pref_string_content;
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
      pref_string_title = gPref_string_desc + ".uri";

      var uri = Components.classes["@mozilla.org/supports-string;1"]
        .createInstance(Components.interfaces.nsISupportsString);
      uri.data = ldapUrl.spec;
      gPrefInt.setComplexValue(pref_string_title,
                               Components.interfaces.nsISupportsString, uri);

      pref_string_content = results;
      pref_string_title = gPref_string_desc + ".maxHits";
      if (pref_string_content != kDefaultMaxHits) {
        gPrefInt.setIntPref(pref_string_title, pref_string_content);
      } else {
        try {
          gPrefInt.clearUserPref(pref_string_title);
        } catch (ex) {
        }
      }

      pref_string_title = gPref_string_desc + ".auth.dn";
      var dnWString = Components.classes["@mozilla.org/supports-string;1"]
        .createInstance(Components.interfaces.nsISupportsString);
      dnWString.data = dn;
      gPrefInt.setComplexValue(pref_string_title,
                               Components.interfaces.nsISupportsString, 
                               dnWString);

      // We don't actually allow the password to be saved in the preferences;
      // this preference is (effectively) ignored by the current code base.  
      // It's here because versions of Mozilla 1.0 and earlier (maybe 1.1alpha
      // too?) would blow away the .auth.dn preference if .auth.savePassword
      // is not set.  To avoid trashing things for users who switch between
      // versions, we'll set it.  Once the versions in question become 
      // obsolete enough, this workaround can be gotten rid of.
      //
      try { 
        gPrefInt.setBoolPref(gPref_string_desc + ".auth.savePassword", true);
      } catch (ex) {
        // if this fails, we can live with that; keep going
      }

      window.opener.gNewServer = description;
      window.opener.gNewServerString = gPref_string_desc;
      // set window.opener.gUpdate to true so that LDAP Directory Servers
      // dialog gets updated
      window.opener.gUpdate = true; 
    } else {
      var addressBookBundle = document.getElementById("bundle_addressBook");
      var errorMsg = addressBookBundle.getString(errorValue);
      alert(errorMsg);
    }
  } catch (outer) {
    dump("Internal error in pref-directory-add.js:onAccept() " + outer + "\n");
  }
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
