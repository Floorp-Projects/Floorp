var gPrefstring = "ldap_2.servers.";
var gPref_string_desc = "";
var gPrefInt = null;
var gCurrentDirectory = null;
var gCurrentDirectoryString = null;
var gPortNumber = 389;
var gMaxHits = 100;

function Startup()
{
  if ( window.arguments && window.arguments[0] ) {
    gCurrentDirectory = window.arguments[0].selectedDirectory;
    gCurrentDirectoryString = window.arguments[0].selectedDirectoryString;
    fillSettings();
  }
  else {
    fillDefaultSettings();
  }
  doSetOKCancel(onOK, onCancel);
}

function fillSettings()
{
  try {
    gPrefInt = Components.classes["@mozilla.org/preferences;1"];
    gPrefInt = gPrefInt.getService(Components.interfaces.nsIPref);
  }
  catch (ex) {
    dump("failed to get prefs service!\n");
    gPrefInt = null;
  }
  try {
    var ldapUrl = Components.classes["@mozilla.org/network/ldap-url;1"];
    ldapUrl = ldapUrl.getService(Components.interfaces.nsILDAPURL);
  }
  catch (ex) {
    dump("failed to get ldap url service!\n");
    ldapUrl = null;
  }
  try{
    var prefValue = gPrefInt.CopyCharPref(gCurrentDirectoryString +".description");
  }
  catch(ex){
    prefValue="";
  }
  
  if (ldapUrl && prefValue)
  {
    document.getElementById("description").value = prefValue;
    try{
      prefValue = gPrefInt.CopyCharPref(gCurrentDirectoryString +".uri");
    }
    catch(ex){
      prefValue="";
    }
    if (prefValue)
    {
      ldapUrl.spec = prefValue;
      document.getElementById("hostname").value = ldapUrl.host;
      document.getElementById("port").value = ldapUrl.port;
      document.getElementById("basedn").value = ldapUrl.dn;
      document.getElementById("search").value = ldapUrl.filter;
      switch(ldapUrl.scope)
      {
        case 1:
          document.getElementById("one").checked = true; break;
        case 2:
          document.getElementById("sub").checked = true; break;
      }
    }
    try {
      prefValue = gPrefInt.GetIntPref(gCurrentDirectoryString+ ".maxHits");
    }
    catch(ex) {
      prefValue = gMaxHits;
    }
    document.getElementById("results").value = prefValue;
  }
}

function fillDefaultSettings()
{
  document.getElementById("port").value = gPortNumber;
  document.getElementById("results").value = gMaxHits;
  document.getElementById("sub").checked = true;
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
      user_Id = gPrefInt.GetIntPref("ldap_2.user_id");
    }
    catch(ex){ 
      user_Id = 0;
    }
    ++user_Id;
    temp = "user_directory_" + user_Id;
    str = temp;
    try {
      gPrefInt.SetIntPref("ldap_2.user_id", user_Id);
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
      temp = gPrefInt.CopyCharPref(gPrefstring+gPref_string_desc+".description");
    } catch(e){}
    if (temp)
      gPref_string_desc += str[0];
  }
} 
 
function onOK()
{
  var pref_string_content = "";
  var pref_string_title = "";

  if (!gPrefInt) {
    try {
      gPrefInt = Components.classes["@mozilla.org/preferences;1"];
      gPrefInt = gPrefInt.getService(Components.interfaces.nsIPref);
    }
    catch (ex) {
      dump("failed to get prefs service!\n");
      gPrefInt = null;
    }
  }

  try {
    var ldapUrl = Components.classes["@mozilla.org/network/ldap-url;1"];
    ldapUrl = ldapUrl.getService(Components.interfaces.nsILDAPURL);
  }
  catch (ex) {
    dump("failed to get ldap url service!\n");
    ldapUrl = null;
  }

  var description = document.getElementById("description").value;
  var hostname = document.getElementById("hostname").value;
  gPref_string_desc = description;
  if (gPref_string_desc && hostname) {
  pref_string_content = gPref_string_desc;
  if (gCurrentDirectory && gCurrentDirectoryString)
  {
    gPref_string_desc = gCurrentDirectoryString;
  }
  else
  {
    createUniqueServername();
    gPref_string_desc = gPrefstring + gPref_string_desc;
  }

  pref_string_title = gPref_string_desc +"." + "description";
  gPrefInt.SetCharPref(pref_string_title, pref_string_content);
  
  ldapUrl.host = hostname;
  pref_string_content = document.getElementById("basedn").value;
  ldapUrl.dn = pref_string_content;
  pref_string_content = document.getElementById("port").value;
  ldapUrl.port = pref_string_content;
  pref_string_content = document.getElementById("search").value;
  ldapUrl.filter = pref_string_content;
  if (document.getElementById("one").checked)
  {
    ldapUrl.scope = 1;
  }
  else {
      ldapUrl.scope = 2;
  }
  pref_string_title = gPref_string_desc + ".uri";
  gPrefInt.SetCharPref(pref_string_title, ldapUrl.spec);
  pref_string_content = document.getElementById("results").value;
  if (pref_string_content != gMaxHits) {
    pref_string_title = gPref_string_desc + ".maxHits";
    gPrefInt.SetIntPref(pref_string_title, pref_string_content);
  }
  pref_string_title = gPref_string_desc + ".auth.enabled";
  try{
    pref_string_content = gPrefInt.GetBoolPref(pref_string_title); 
  }
  catch(ex) {
    pref_string_content = false;
  }
  window.opener.gNewServer = description;
  window.opener.gNewServerString = gPref_string_desc;  
  window.close();
  }
  else
  {
    var addressBookBundle = document.getElementById("bundle_addressBook");
    errorMsg = addressBookBundle.getString("errorMessage");
    alert(errorMsg);
  }

}

function onCancel()
{
  window.close();
}
