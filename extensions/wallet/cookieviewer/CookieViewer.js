/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Ben Goodger
 */

// global variables
var cookiemanager         = null;          // cookiemanager interface
var permissionmanager     = null;          // permissionmanager interface
var cookies               = [];            // array of cookies
var cpermissions           = [];           // array of cookie permissions
var ipermissions           = [];           // array of image permissions
var images                = [];            // array of images (NEW OBJECT)
var deleted_cookies       = [];
var deleted_permissions   = [];
var deleted_images        = [];
var deleted_cookies_count            = 0;
var deleted_cookie_permissions_count = 0;
var deleted_image_permissions_count  = 0;
var cookie_permissions_count         = 0;
var image_permissions_count          = 0;
var cookieType                       = 0;
var imageType                        = 1;
var gDateService = null;

// for dealing with the interface:
var gone_c                = "";
var gone_p                = "";
var gone_i                = "";
// string bundle
var bundle                = null;
// CHANGE THIS WHEN MOVING FILES - strings localization file!
var JS_STRINGS_FILE = "chrome://communicator/locale/wallet/CookieViewer.properties";
const nsScriptableDateFormat_CONTRACTID = "@mozilla.org/intl/scriptabledateformat;1";
const nsIScriptableDateFormat = Components.interfaces.nsIScriptableDateFormat;
    
// function : <CookieViewer.js>::Startup();
// purpose  : initialises the cookie viewer dialog
function Startup()
{
  // xpconnect to cookiemanager interface
  cookiemanager = Components.classes["@mozilla.org/cookiemanager;1"].getService();
  cookiemanager = cookiemanager.QueryInterface(Components.interfaces.nsICookieManager);
  // xpconnect to permissionmanager interface
  permissionmanager = Components.classes["@mozilla.org/permissionmanager;1"].getService();
  permissionmanager = permissionmanager.QueryInterface(Components.interfaces.nsIPermissionManager);
  // intialise string bundle
  if (!gDateService) {
    gDateService = Components.classes[nsScriptableDateFormat_CONTRACTID]
      .getService(nsIScriptableDateFormat);
  }
  bundle = srGetStrBundle(JS_STRINGS_FILE);

  // install imageblocker tab if instructed to do so by the "imageblocker.enabled" pref
  try {
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);
    try {
      if (pref.getBoolPref("imageblocker.enabled")) {
        var element;
        element = document.getElementById("imagesTab");
        element.setAttribute("hidden","false" );
        element = document.getElementById("images");
        element.setAttribute("hidden","false" );
      }
    } catch(e) {
    }
    try {
      var tab = window.arguments[0];
      var element2 = document.getElementById("tabbox");
      if (tab == "0") {
        element = document.getElementById("cookiesTab");
        element2.selectedTab = element;
        element = document.getElementById("imagesTab");
        element.setAttribute("hidden","true" );
      } else {
        element = document.getElementById("cookieviewer");
        element.setAttribute("title", bundle.GetStringFromName("imageTitle"));
        element = document.getElementById("imagesTab");
        element2.selectedTab = element;
        element = document.getElementById("serversTab");
        element.setAttribute("hidden","true" );
        element = document.getElementById("cookiesTab");
        element.setAttribute("hidden","true" );
      }
    } catch(e) {
    }
  } catch (ex) {
    dump("failed to get prefs service!\n");
    pref = null;
  }

 
  loadCookies();
  loadPermissions();
  doSetOKCancel(onOK, null);
  window.sizeToContent();
}

/*** =================== COOKIES CODE =================== ***/

// function : <CookieViewer.js>::AddCookieToList();
// purpose  : creates an array of cookie objects
function AddCookieToList(count, name, value, isDomain, host, path, isSecure, expires) {
  cookies[count] = new Cookie();
  cookies[count].number     = count;
  cookies[count].name       = name;
  cookies[count].value      = value;
  cookies[count].isDomain   = isDomain;
  cookies[count].host       = host;
  cookies[count].path       = path;
  cookies[count].isSecure   = isSecure;
  cookies[count].expires    = expires;
}

// function : <CookieViewer.js>::Cookie();
// purpose  : an home-brewed object that represents a individual cookie in the stream
function Cookie(number,name,value,isDomain,host,path,isSecure,expires)
{
  this.number       = ( number ) ? number : null;
  this.name         = ( name ) ? name : null;
  this.value        = ( value ) ? value : null;
  this.isDomain     = ( isDomain ) ? isDomain : null;
  this.host         = ( host ) ? host : null;
  this.path         = ( path ) ? path : null;
  this.isSecure     = ( isSecure ) ? isSecure : null;
  this.expires      = ( expires ) ? expires : null;
}


// function : <CookieViewer.js>::loadCookies();
// purpose  : loads the list of cookies into the cookie list treeview
function loadCookies()
{
  var cookieString;
  var cookieArray  = [];

  var enumerator = cookiemanager.enumerator;
  var count = 0;
  while (enumerator.hasMoreElements()) {
    var nextCookie = enumerator.getNext();
    nextCookie = nextCookie.QueryInterface(Components.interfaces.nsICookie);

    var name = nextCookie.name;
    var value = nextCookie.value;
    var isDomain = nextCookie.isDomain;
    var host = nextCookie.host;
    var path = nextCookie.path;
    var isSecure = nextCookie.isSecure;
    var expires = nextCookie.expires;

    AddCookieToList
      (count, name, value, isDomain, host, path, isSecure, expires);
    if(host.charAt(0) == ".") { // get rid of the ugly dot on the start of some domains
      host = host.substring(1,host.length);
    }
    AddItem("cookieList", [host, name], "cookietree_", count++);
  }
  Wallet_ColumnSort('0', 'cookieList');
  if (count == 0) {
    document.getElementById("removeAllCookies").setAttribute("disabled","true");
  }
}

function CookieColumnSort(column) {
  var number, cookie, id;
  var cookietree = document.getElementById("cookietree");
  var selected = cookietree.selectedIndex;
  var cookielist = document.getElementById('cookieList');
  var childnodes = cookielist.childNodes;

  if (selected >= 0) {
    cookie = childnodes[selected];
    id = cookie.getAttribute("id");
    number = parseInt(id.substring("cookietree_".length,id.length));
  }

  Wallet_ColumnSort(column, 'cookieList');

  var length = childnodes.length;
  if (selected >= 0) {
    for (i=0; i<length; i++){
      cookie = childnodes[i];
      id = cookie.getAttribute("id");
      var thisNumber = parseInt(id.substring("cookietree_".length,id.length));
      if (thisNumber == number) {
        cookietree.selectedIndex = i;
        break;
      }
    }
  }

}

// function : <CookieViewer.js>::ViewSelectedCookie();
// purpose  : displays information about the selected cookie in the info fieldset
function ViewCookieSelected( e ) 
{
  var cookie = null;
  var cookietree = document.getElementById("cookietree");
  var selItemsMax = false;
  if(cookietree.nodeName != "tree")
    return false;
  if(cookietree.selectedItems.length > 1)
    selItemsMax = true;
  if( cookietree.selectedItems.length )
    document.getElementById("removeCookies").removeAttribute("disabled","true");
    
  if( ( e.type == "keypress" || e.type == "select" ) && e.target.selectedItems.length )
    cookie = cookietree.selectedItems[0];
  if( e.type == "click" ) 
    cookie = e.target.parentNode.parentNode;

  if( !cookie || cookie.getAttribute("id").indexOf("cookietree_") == -1)
    return false;
  var idx = parseInt(cookie.getAttribute("id").substring("cookietree_".length,cookie.getAttribute("id").length));
  for (var x=0; x<cookies.length; x++) {
    if (cookies[x].number == idx) {
      idx = x;
      break;
    }
  }
  var props = [cookies[idx].number, cookies[idx].name, cookies[idx].value, 
               cookies[idx].isDomain, cookies[idx].host, cookies[idx].path,
               cookies[idx].isSecure, cookies[idx].expires];

  var rows =
    [null,"ifl_name","ifl_value","ifl_isDomain","ifl_host","ifl_path","ifl_isSecure","ifl_expires"];
  var value;
  var field;
  for(var i = 1; i < props.length; i++)
  {
    if(rows[i] == "ifl_isDomain") {
      field = document.getElementById("ifl_isDomain");
      value = cookies[idx].isDomain ?
              bundle.GetStringFromName("domainColon") :
              bundle.GetStringFromName("hostColon");
    } else if (rows[i] == "ifl_isSecure") {
      field = document.getElementById("ifl_isSecure");
      value = cookies[idx].isSecure ?
              bundle.GetStringFromName("yes") : bundle.GetStringFromName("no");
    } else if (rows[i] == "ifl_expires") {
      field = document.getElementById("ifl_expires");
      var date = new Date(1000*cookies[idx].expires);
      var localetimestr =  gDateService.FormatDateTime(
                              "", gDateService.dateFormatLong,
                              gDateService.timeFormatSeconds,
                              date.getFullYear(), date.getMonth()+1,
                              date.getDate(), date.getHours(),
                              date.getMinutes(), date.getSeconds());
      value = cookies[idx].expires
              ? localetimestr
              //? date.toLocaleString()
              : bundle.GetStringFromName("AtEndOfSession");
    } else {
      field = document.getElementById(rows[i]);
      value = props[i];
    }
    if (selItemsMax && rows[i] != "ifl_isDomain") {
      value = ""; // clear field if multiple selections
    }
    field.setAttribute("value", value);
    if(rows[i] == "ifl_expires") break;
  }
  return true;
}

// function : <CookieViewer.js>::DeleteCookieSelected();
// purpose  : deletes all the cookies that are selected
function DeleteCookieSelected() {
  // delete selected item
  var cookietree = document.getElementById("cookietree");
  deleted_cookies_count += cookietree.selectedItems.length;
  var newIndex = cookietree.selectedIndex;
  gone_c += DeleteItemSelected("cookietree", "cookietree_", "cookieList");
  // set fields
  var rows = ["ifl_name","ifl_value","ifl_host","ifl_path","ifl_isSecure","ifl_expires"];
  for(var k = 0; k < rows.length; k++) 
  {
    var row = document.getElementById(rows[k]);
    row.setAttribute("value","");
  }
  var netCookieCount = cookies.length - deleted_cookies_count;
  if (netCookieCount) {
    cookietree.selectedIndex =
      (newIndex < netCookieCount) ? newIndex : netCookieCount-1;
  }
  if( !cookietree.selectedItems.length ) {
    if( !document.getElementById("removeCookies").disabled ) {
      document.getElementById("removeCookies").setAttribute("disabled", "true")
    }
  }
  if (netCookieCount <= 0) {
    document.getElementById("removeAllCookies").setAttribute("disabled","true");
  }
}

// function : <CookieViewer.js>::DeleteAllCookies();
// purpose  : deletes all the cookies
function DeleteAllCookies() {
  // delete selected item
  gone_c += DeleteAllItems(cookies.length, "cookietree_", "cookieList");
  // set fields
  var rows = ["ifl_name","ifl_value","ifl_host","ifl_path","ifl_isSecure","ifl_expires"];
  for(var k = 0; k < rows.length; k++) 
  {
    var row = document.getElementById(rows[k]);
    row.setAttribute("value","");
  }
  if( !document.getElementById("removeCookies").disabled ) {
    document.getElementById("removeCookies").setAttribute("disabled", "true")
  }
  document.getElementById("removeAllCookies").setAttribute("disabled","true");
}

// keypress pass-thru
function HandleKeyPress( e )
{
  switch ( e.keyCode )
  {
  case 13:  // enter
  case 32:  // spacebar
    ViewCookieSelected( e );
    break;
  case 46:  // delete
    DeleteCookieSelected();
    break;
  default:
    break;
  }
}

// will restore deleted cookies when I get around to filling it in.
function RestoreCookies()
{
  // todo  
}

/*** =================== PERMISSIONS CODE =================== ***/

// function : <CookieViewer.js>::AddPermissionToList();
// purpose  : creates an array of permission objects
function AddPermissionToList(count, host, type, capability) {
  if (type == cookieType) {
    cpermissions[count] = new Permission();
    cpermissions[count].number = count;
    cpermissions[count].host = host;
    cpermissions[count].type = type;
    cpermissions[count].capability = capability;
  } else if (type == imageType) {
    ipermissions[count] = new Permission();
    ipermissions[count].number = count;
    ipermissions[count].host = host;
    ipermissions[count].type = type;
    ipermissions[count].capability = capability;
  }
}

// function : <CookieViewer.js>::Permission();
// purpose  : an home-brewed object that represents a individual permission in the stream
function Permission(number, host, type, capability)
{
  this.number       = (number) ? number : null;
  this.host         = (host) ? host : null;
  this.type         = (type) ? type : null;
  this.capability   = (capability) ? capability : null;
}

// function : <CookieViewer.js>::loadPermissions();
// purpose  : loads the list of permissions into the permission list treeview
function loadPermissions()
{
  var permissionString;
  var permissionArray  = [];

  var enumerator = permissionmanager.enumerator;
  var contentStr;
  while (enumerator.hasMoreElements()) {
    var nextPermission = enumerator.getNext();
    nextPermission = nextPermission.QueryInterface(Components.interfaces.nsIPermission);

    var host = nextPermission.host;
    var type = nextPermission.type;
    var capability = nextPermission.capability;
    if(host.charAt(0) == ".") {  // get rid of the ugly dot on the start of some domains
      host = host.substring(1,host.length);
    }
    if (type == cookieType) {
      contentStr = bundle.GetStringFromName(capability ? "can" : "cannot");
      AddPermissionToList(cookie_permissions_count, host, type, capability);
      AddItem("cookiePermList", [host, contentStr], "cookiepermtree_", cookie_permissions_count++);
    } else if (type == imageType) {
      contentStr = bundle.GetStringFromName(capability ? "canImages" : "cannotImages");
      AddPermissionToList(image_permissions_count, host, type, capability);
      AddItem("imagePermList", [host, contentStr], "imagepermtree_", image_permissions_count++);
    }
  }
  if (cookie_permissions_count == 0) {
    document.getElementById("removeAllPermissions").setAttribute("disabled","true");
  }
  if (image_permissions_count == 0) {
    document.getElementById("removeAllImages").setAttribute("disabled","true");
  }

}

function ViewCookiePermissionSelected()
{
  var cookiepermtree = document.getElementById("cookiepermissionstree");
  if( cookiepermtree.selectedItems.length )
    document.getElementById("removePermissions").removeAttribute("disabled","true");
}

function DeleteCookiePermissionSelected()
{
  var cookiepermissiontree = document.getElementById("cookiepermissionstree");
  deleted_cookie_permissions_count += cookiepermissiontree.selectedItems.length;
  var newIndex = cookiepermissiontree.selectedIndex;
  gone_p += DeleteItemSelected('cookiepermissionstree', 'cookiepermtree_', 'cookiePermList');
  var netCookiePermissionCount =
    cookie_permissions_count - deleted_cookie_permissions_count;
  if (netCookiePermissionCount) {
    cookiepermissiontree.selectedIndex =
      (newIndex < netCookiePermissionCount) ? newIndex : netCookiePermissionCount-1;
  }
  if( !cookiepermissiontree.selectedItems.length ) {
    if( !document.getElementById("removePermissions").disabled ) {
      document.getElementById("removePermissions").setAttribute("disabled", "true")
    }
  }
  if (netCookiePermissionCount <= 0) {
    document.getElementById("removeAllPermissions").setAttribute("disabled","true");
  }
}

function DeleteAllCookiePermissions() {
  // delete selected item
  gone_p += DeleteAllItems(cookie_permissions_count, "cookiepermtree_", "cookiePermList");
  if( !document.getElementById("removePermissions").disabled ) {
    document.getElementById("removePermissions").setAttribute("disabled", "true")
  }
  document.getElementById("removeAllPermissions").setAttribute("disabled","true");
}

/*** =================== IMAGES CODE =================== ***/

function ViewImagePermissionSelected()
{
  var imagepermtree = document.getElementById("imagepermissionstree");
  if( imagepermtree.selectedItems.length )
    document.getElementById("removeImages").removeAttribute("disabled","true");
}

function DeleteImagePermissionSelected()
{
  var imagepermissiontree = document.getElementById("imagepermissionstree");
  deleted_image_permissions_count += imagepermissiontree.selectedItems.length;
  var newIndex = imagepermissiontree.selectedIndex;
  gone_i += DeleteItemSelected('imagepermissionstree', 'imagepermtree_', 'imagePermList');
  var netImagePermissionCount =
    image_permissions_count - deleted_image_permissions_count;
  if (netImagePermissionCount) {
    imagepermissiontree.selectedIndex =
      (newIndex < netImagePermissionCount) ? newIndex : netImagePermissionCount-1;
  }
  if( !imagepermissiontree.selectedItems.length ) {
    if( !document.getElementById("removeImages").disabled ) {
      document.getElementById("removeImages").setAttribute("disabled", "true")
    }
  }
  if (netImagePermissionCount <= 0) {
    document.getElementById("removeAllImages").setAttribute("disabled","true");
  }
}

function DeleteAllImagePermissions() {
  // delete selected item
  gone_i += DeleteAllItems(image_permissions_count, "imagepermtree_", "imagePermList");
  if( !document.getElementById("removeImages").disabled ) {
    document.getElementById("removeImages").setAttribute("disabled", "true")
  }
  document.getElementById("removeAllImages").setAttribute("disabled","true");
}

/*** =================== GENERAL CODE =================== ***/

// function : <CookieViewer.js>::doOKButton();
// purpose  : saves the changed settings and closes the dialog.
function onOK(){

  var deletedCookies = [];
  deletedCookies = gone_c.split(",");
  var cookieCount;
  for (cookieCount=0; cookieCount<deletedCookies.length-1; cookieCount++) {
    cookiemanager.remove(cookies[deletedCookies[cookieCount]].host,
                                cookies[deletedCookies[cookieCount]].name,
                                cookies[deletedCookies[cookieCount]].path,
                                document.getElementById("checkbox").checked);
  }

  var deletedCookiePermissions = [];
  deletedCookiePermissions = gone_p.split(",");
  var cperm_count;
  for (cperm_count=0;
       cperm_count<deletedCookiePermissions.length-1;
       cperm_count++) {
    permissionmanager.remove
      (cpermissions[deletedCookiePermissions[cperm_count]].host, cookieType);
  }

  var deletedImagePermissions = [];
  deletedImagePermissions = gone_i.split(",");
  var iperm_count;
  for (iperm_count=0;
       iperm_count<deletedImagePermissions.length-1;
       iperm_count++) {
    permissionmanager.remove
      (ipermissions[deletedImagePermissions[iperm_count]].host, imageType);
  }
  return true;
}

/*** =================== TREE MANAGEMENT CODE =================== ***/

// function : <CookieViewer.js>::AddItem();
// purpose  : utility function for adding items to a tree.
function AddItem(children,cells,prefix,idfier)
{
  var kids = document.getElementById(children);
  var item  = document.createElement("treeitem");
  var row   = document.createElement("treerow");
  for(var i = 0; i < cells.length; i++)
  {
    var cell  = document.createElement("treecell");
    cell.setAttribute("class", "propertylist");
    cell.setAttribute("label", cells[i])
    row.appendChild(cell);
  }
  item.appendChild(row);
  item.setAttribute("id",prefix + idfier);
  kids.appendChild(item);
}

// function : <CookieViewer.js>::DeleteItemSelected();
// purpose  : deletes all the signons that are selected
function DeleteItemSelected(tree, prefix, kids) {
  var i;
  var delnarray = [];
  var rv = "";
  var cookietree = document.getElementById(tree);
  var selitems = cookietree.selectedItems;
  for(i = 0; i < selitems.length; i++) 
  { 
    delnarray[i] = document.getElementById(selitems[i].getAttribute("id"));
    var itemid = parseInt(selitems[i].getAttribute("id").substring(prefix.length,selitems[i].getAttribute("id").length));
    rv += (itemid + ",");
  }
  for(i = 0; i < delnarray.length; i++) 
  { 
    document.getElementById(kids).removeChild(delnarray[i]);
  }
  return rv;
}

// function : <CookieViewer.js>::DeleteAllItems();
// purpose  : deletes all the items
function DeleteAllItems(length, prefix, kids) {
  var delnarray = [];
  var rv = "";
  for(var i = 0; i < length; i++) 
  { 
    if (document.getElementById(prefix+i) != null) {
      document.getElementById(kids).removeChild(document.getElementById(prefix+i));
      rv += (i + ",");
    }
  }

  return rv;
}
