var gPrefInt = null;
var gAvailDirectories = null;
var gCurrentDirectoryServer = null;
var gCurrentDirectoryServerId = null;
var gRefresh = false;
var gNewServer = null;
var gNewServerString = null;
var gFromGlobalPref = false;
var gUpdate = false;
var gDeletedDirectories = new Array();

function onEditDirectories()
{
  var args = {fromGlobalPref: gFromGlobalPref};
  window.openDialog("chrome://messenger/content/addressbook/pref-editdirectories.xul",
                    "editDirectories", "chrome,modal=yes,resizable=no", args);
  if (gRefresh)
  {
    var popup = document.getElementById("directoriesListPopup"); 
    if (popup) 
    { 
       while (popup.childNodes.length)
         popup.removeChild(popup.childNodes[0]);
    } 
    gAvailDirectories = null;
    LoadDirectories(popup);
    gRefresh = false;
  }
}

function enableAutocomplete()
{
  var autocompleteLDAP = document.getElementById("autocompleteLDAP");  
  var directoriesList =  document.getElementById("directoriesList"); 
  var directoriesListPopup = document.getElementById("directoriesListPopup");
  var editButton = document.getElementById("editButton");
//  var autocompleteSkipDirectory = document.getElementById("autocompleteSkipDirectory");

  if (autocompleteLDAP.checked) {
    directoriesList.removeAttribute("disabled");
    directoriesListPopup.removeAttribute("disabled");
    editButton.removeAttribute("disabled");
//    autocompleteSkipDirectory.removeAttribute("disabled");
  }
  else {
    directoriesList.setAttribute("disabled", true);
    directoriesListPopup.setAttribute("disabled", true);
    editButton.setAttribute("disabled", true);
//    autocompleteSkipDirectory.setAttribute("disabled", true);
  }
  gFromGlobalPref = true;
  LoadDirectories(directoriesListPopup);
}

function setupDirectoriesList()
{
  var override = document.getElementById("identity.overrideGlobalPref").getAttribute("value");
  var autocomplete = document.getElementById("ldapAutocomplete");

  if(override == "true")
    autocomplete.selectedItem = document.getElementById("directories");
  else
    autocomplete.selectedItem = document.getElementById("useGlobalPref");

  var directoriesList = document.getElementById("directoriesList");
  var directoryServer = 
        document.getElementById("identity.directoryServer").getAttribute('value');
  try {
    var directoryServerString = gPrefInt.CopyUnicharPref(directoryServer + ".description");
  }
  catch(ex) {
    var addressBookBundle = document.getElementById("bundle_addressBook");
    directoryServerString = addressBookBundle.getString("directoriesListItemNone");
  }
  directoriesList.value = directoryServer;
  directoriesList.label = directoryServerString;
  gFromGlobalPref = false;
}

function createDirectoriesList(flag) 
{
  gFromGlobalPref = flag;
  var directoriesListPopup = document.getElementById("directoriesListPopup");

  if (directoriesListPopup) {
    LoadDirectories(directoriesListPopup);
  }
}

function migrate(pref_string)
{
  if (!gPrefInt) { 
    try {
      gPrefInt = Components.classes["@mozilla.org/preferences;1"];
      gPrefInt = gPrefInt.getService(Components.interfaces.nsIPref);
    }
    catch (ex) {
      gPrefInt = null;
    }
  }
  try{
    var migrated = gPrefInt.GetBoolPref("ldap_2.prefs_migrated");
  }
  catch(ex){
    migrated = false;
  }
  if (!migrated) {
  try {
    var ldapUrl = Components.classes["@mozilla.org/network/ldap-url;1"];
    ldapUrl = ldapUrl.getService(Components.interfaces.nsILDAPURL);
  }
  catch (ex) {
    ldapUrl = null;
  }
  try {
    var ldapService = Components.classes[
        "@mozilla.org/network/ldap-service;1"].
        getService(Components.interfaces.nsILDAPService);
  }
  catch (ex)
  { 
    dump("failed to get ldapService \n");
    ldapService = null;
  }
  try{
    ldapUrl.host = gPrefInt.CopyCharPref(pref_string + ".serverName");
    if(ldapService) {
      var base = gPrefInt.CopyUnicharPref(pref_string + ".searchBase"); 
      ldapUrl.dn = ldapService.UCS2toUTF8(base);
    }
  }
  catch(ex) {
  }
  try {
    var port = gPrefInt.CopyCharPref(pref_string + ".port");
  }
  catch(ex) {
    port = 389;
  }
  ldapUrl.port = port;
  ldapUrl.scope = 0;
  gPrefInt.SetUnicharPref(pref_string + ".uri", ldapUrl.spec);
  gPrefInt.SetBoolPref("ldap_2.prefs_migrated", true);
  }
}

function LoadDirectories(popup)
{
  var children;
  var enabled = false;
  var description = "";
  var item;
  var formElement;
  var j=0;
  var arrayOfDirectories = null;
  var position = 0;
  var dirType = 1;
  if (!gPrefInt) { 
    try {
      gPrefInt = Components.classes["@mozilla.org/preferences;1"];
      gPrefInt = gPrefInt.getService(Components.interfaces.nsIPref);
    }
    catch (ex) {
      gPrefInt = null;
    }
  }
  children = gPrefInt.CreateChildList("ldap_2.servers");
  if (children && !gAvailDirectories) {
    arrayOfDirectories = children.split(';');
    gAvailDirectories = new Array();
    for (var i=0; i<arrayOfDirectories.length; i++)
    {
      if ((arrayOfDirectories[i] != "ldap_2.servers.pab") && 
        (arrayOfDirectories[i] != "ldap_2.servers.history")) {
        try{
          position = gPrefInt.GetIntPref(arrayOfDirectories[i]+".position");
        }
        catch(ex){
          position = 1;
        }
        try{
          dirType = gPrefInt.GetIntPref(arrayOfDirectories[i]+".dirType");
        }
        catch(ex){
          dirType = 1;
        }
        if ((position != 0) && (dirType == 1)) {
          try{
            description = gPrefInt.CopyUnicharPref(arrayOfDirectories[i]+".description");
          }
          catch(ex){
            description="";
          }
          if (description != "") {
            if (popup) {
              item=document.createElement("menuitem");
              item.setAttribute("label", description);
              item.setAttribute("value", arrayOfDirectories[i]);
              popup.appendChild(item);
            }
            migrate(arrayOfDirectories[i]);
            gAvailDirectories[j] = new Array(2);
            gAvailDirectories[j][0] = arrayOfDirectories[i];
            gAvailDirectories[j][1] = description;
            j++;
          }
        }
      }
    }
    if (popup && !gFromGlobalPref) 
    {
      item=document.createElement("menuitem");
      var addressBookBundle = document.getElementById("bundle_addressBook");
      var directoryName = addressBookBundle.getString("directoriesListItemNone");
      item.setAttribute("label", directoryName);
      item.setAttribute("value", "");
      popup.appendChild(item);
      if (gRefresh)
        setupDirectoriesList();
    }
    if (popup && gFromGlobalPref) {
      var pref_string_title = "ldap_2.autoComplete.directoryServer";
      try {
        var directoryServer = gPrefInt.CopyCharPref(pref_string_title);
      }
      catch (ex)
      {
        directoryServer = "";
      }
      if (directoryServer != "")
      {
        pref_string_title = directoryServer + ".description";
        try {
          description = gPrefInt.CopyUnicharPref(pref_string_title);
        }
        catch (ex) {
          description = "";
        } 
	  }
	  var directoriesList =  document.getElementById("directoriesList");
      if ((directoryServer != "") && (description != ""))
      {
        directoriesList.label = description;
        directoriesList.value = directoryServer;
      }
      else if(gAvailDirectories.length >= 1) {
         directoriesList.label = gAvailDirectories[0][1];
         directoriesList.value = gAvailDirectories[0][0];
      }
      else {
        directoriesList.label = "";
        directoriesList.value = null;
      }
	}
  }
}

function onInitEditDirectories()
{
  var directoriesTree_root = document.getElementById("directoriesTree_root");
  gFromGlobalPref = window.arguments[0].fromGlobalPref;
  if (directoriesTree_root) {
    LoadDirectoriesTree(directoriesTree_root);
  }
  doSetOKCancel(onOK, cancel);
}

function LoadDirectoriesTree(tree)
{
  var item;
  var row;
  var cell;
  
  LoadDirectories();
  if (tree && gAvailDirectories)
  {
    for (var i=0; i<gAvailDirectories.length; i++)
    {
      item = document.createElement('treeitem');
      row  = document.createElement('treerow');
      cell = document.createElement('treecell');

      cell.setAttribute('label', gAvailDirectories[i][1]);
      cell.setAttribute('string', gAvailDirectories[i][0]);

      row.appendChild(cell);
      item.appendChild(row);
      tree.appendChild(item);
    }
  }
}

function selectDirectory()
{
  var directoriesTree = document.getElementById("directoriesTree");
  if(directoriesTree && directoriesTree.selectedItems 
     && directoriesTree.selectedItems.length)
  {
    gCurrentDirectoryServer = 
	  directoriesTree.selectedItems[0].firstChild.firstChild.getAttribute('label');
    gCurrentDirectoryServerId = 
	  directoriesTree.selectedItems[0].firstChild.firstChild.getAttribute('string');
  }
  else
  {
    gCurrentDirectoryServer = null;
    gCurrentDirectoryServerId = null;
  }

  var editButton = document.getElementById("editButton");
  var removeButton = document.getElementById("removeButton");
  if(gCurrentDirectoryServer && gCurrentDirectoryServerId) {
    editButton.removeAttribute("disabled");
    removeButton.removeAttribute("disabled");
  }
  else {
    editButton.setAttribute("disabled", true);
    removeButton.setAttribute("disabled", true);
  }
}

function newDirectory()
{
  window.openDialog("chrome://messenger/content/addressbook/pref-directory-add.xul",
                    "addDirectory", "chrome,modal=yes,resizable=no");
  if(gNewServer && gNewServerString) {
    var tree = document.getElementById("directoriesTree_root");
    var item = document.createElement('treeitem');
    var row  = document.createElement('treerow');
    var cell = document.createElement('treecell');

    cell.setAttribute('label', gNewServer);
    cell.setAttribute('string', gNewServerString);

    row.appendChild(cell);
    item.appendChild(row);
    tree.appendChild(item);
    gNewServer = null;
    gNewServerString = null;
    window.opener.gRefresh = true;
  }
}

function editDirectory()
{
  var args = { selectedDirectory: null,
               selectedDirectoryString: null,
               result: false};
  if(gCurrentDirectoryServer && gCurrentDirectoryServerId) {
    args.selectedDirectory = gCurrentDirectoryServer;
    args.selectedDirectoryString = gCurrentDirectoryServerId;
    window.openDialog("chrome://messenger/content/addressbook/pref-directory-add.xul",
                      "editDirectory", "chrome,modal=yes,resizable=no", args);
  }
  if(gUpdate) 
  {
    // directory server properties have changed. So, update the  
    // LDAP Directory Servers dialog.  
    var directoriesTree = document.getElementById("directoriesTree"); 
    var selectedNode = directoriesTree.selectedItems[0]; 
    var row  =  selectedNode.firstChild; 
    var cell =  row.firstChild; 
    cell.setAttribute('label', gNewServer); 
    cell.setAttribute('string', gNewServerString);
    // set gUpdate to false since we have updated the server name in the list.
    gUpdate = false;
    // window.opener is either global pref window or 
    // mail/news account settings window.
    // set window.opener.gRefresh to true such that the 
    // dropdown list box gets updated 
    window.opener.gRefresh = true; 
  } 
}

function removeDirectory()
{
  var directoriesTree = document.getElementById("directoriesTree");
  var directoriesTree_root = document.getElementById("directoriesTree_root");
  var selectedNode = directoriesTree.selectedItems[0];
    
  var nextNode = selectedNode.nextSibling;
  if (!nextNode)
    if (selectedNode.previousSibling)
      nextNode = selectedNode.previousSibling;

  var row  =  selectedNode.firstChild;
  var cell =  row.firstChild;

  if(gCurrentDirectoryServer && gCurrentDirectoryServerId) {
    if(!gPrefInt) { 
      try {
        gPrefInt = Components.classes["@mozilla.org/preferences;1"];
        gPrefInt = gPrefInt.getService(Components.interfaces.nsIPref);
      }
      catch (ex) {
        gPrefInt = null;
      }
    }
    try {
      var directoryServer = gPrefInt.CopyCharPref("ldap_2.autoComplete.directoryServer");
    }
    catch(ex)  {
      directoryServer = "";
    }
    if (gCurrentDirectoryServerId == directoryServer)
      gPrefInt.SetCharPref("ldap_2.autoComplete.directoryServer", "");
    var len= gDeletedDirectories.length;
    gDeletedDirectories[len] = gCurrentDirectoryServerId;
  }
  row.removeChild(cell);
  selectedNode.removeChild(row);
  directoriesTree_root.removeChild(selectedNode);
  if (nextNode) {
    directoriesTree.selectItem(nextNode)
  } 
  window.opener.gRefresh = true;
}

function onOK()
{
  var len = gDeletedDirectories.length;
  for (var i=0; i< len; i++){
    gPrefInt.DeleteBranch(gDeletedDirectories[i]);
  }
  window.close();
}

function cancel()
{
  window.close();
}
