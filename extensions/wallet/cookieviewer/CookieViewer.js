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

/*
 * The cookieList is a sequence of items separated by the BREAK character.  These
 * items are:
 *   empty
 *   number for first cookie
 *   name for first cookie
 *   value for first cookie
 *   domain indicator ("Domain" or "Host") for first cookie
 *   domain or host name for first cookie
 *   path for first cookie
 *   secure indicator ("Yes" or "No") for first cookie
 *   expiration for first cookie
 * with the eight items above repeated for each successive cookie
 */

// global variables
var cookieviewer          = null;          // cookieviewer interface
var cookieList            = [];            // array of cookies (OLD STREAM)
var cookies               = [];            // array of cookeis (NEW OBJECT)
var permissionList        = [];            // array of permissions (OLD STREAM)
var permissions           = [];            // array of permissions (NEW OBJECT)
var imageList             = [];            // array of images (OLD STREAM)
var images                = [];            // array of images (NEW OBJECT)
var deleted_cookies       = [];
var deleted_permissions   = [];
var deleted_images        = [];
var deleted_cookies_count = 0;
var deleted_permissions_count = 0;
var deleted_images_count = 0;
// for dealing with the interface:
var gone_c                = "";
var gone_p                = "";
var gone_i                = "";
// string bundle
var bundle                = null;
// CHANGE THIS WHEN MOVING FILES - strings localization file!
var JS_STRINGS_FILE       = "chrome://communicator/locale/wallet/CookieViewer.properties";
    
// function : <CookieViewer.js>::Startup();
// purpose  : initialises the cookie viewer dialog
function Startup()
{
  // xpconnect to cookieviewer interface
  cookieviewer = Components.classes["@mozilla.org/cookieviewer/cookieviewer-world;1"].createInstance();
  cookieviewer = cookieviewer.QueryInterface(Components.interfaces.nsICookieViewer);
  // intialise string bundle for 
  bundle = srGetStrBundle(JS_STRINGS_FILE);

  // install imageblocker tab if instructed to do so by the "imageblocker.enabled" pref
  try {
    pref = Components.classes['@mozilla.org/preferences;1'];
    pref = pref.getService();
    pref = pref.QueryInterface(Components.interfaces.nsIPref);
    try {
      if (pref.GetBoolPref("imageblocker.enabled")) {
        var element;
        element = document.getElementById("imagesTab");
        element.setAttribute("style","display: inline;" );
        element = document.getElementById("images");
        element.setAttribute("style","display: inline;" );
      }
    } catch(e) {
    }
    try {
      var tab = window.arguments[0];
      if (tab == "0") {
        element = document.getElementById("cookiesTab");
        element.setAttribute("selected","true" );
        element = document.getElementById("panel");
        element.setAttribute("index","0" );
      } else {
        element = document.getElementById("cookieviewer");
        element.setAttribute("title", bundle.GetStringFromName("imageTitle"));
        element = document.getElementById("imagesTab");
        element.setAttribute("selected","true" );
        element = document.getElementById("panel");
        element.setAttribute("index","2" );
      }
    } catch(e) {
    }
  } catch (ex) {
    dump("failed to get prefs service!\n");
    pref = null;
  }

 
  loadCookies();
  loadPermissions();
  loadImages();
  doSetOKCancel(onOK, null);
  window.sizeToContent();
}

/*** =================== COOKIES CODE =================== ***/

// function : <CookieViewer.js>::CreateCookieList();
// purpose  : creates an array of cookie objects from the cookie stream
function CreateCookieList()
{
  count = 0;
  for(i = 1; i < cookieList.length; i+=8)
  {
    cookies[count] = new Cookie();
    cookies[count].number     = cookieList[i+0];
    cookies[count].name       = cookieList[i+1];
    cookies[count].value      = cookieList[i+2];
    cookies[count].domaintype = cookieList[i+3];
    cookies[count].domain     = cookieList[i+4];
    cookies[count].path       = cookieList[i+5];
    cookies[count].secure     = cookieList[i+6];
    cookies[count].expire     = cookieList[i+7];
    count++;
  }  
}

// function : <CookieViewer.js>::Cookie();
// purpose  : an home-brewed object that represents a individual cookie in the stream
function Cookie(number,name,value,domaintype,domain,path,secure,expire)
{
  this.number       = ( number ) ? number : null;
  this.name         = ( name ) ? name : null;
  this.value        = ( value ) ? value : null;
  this.domaintype   = ( domaintype ) ? domaintype : null;
  this.domain       = ( domain ) ? domain : null;
  this.path         = ( path ) ? path : null;
  this.secure       = ( secure ) ? secure : null;
  this.expire       = ( expire ) ? expire : null;
}


// function : <CookieViewer.js>::loadCookies();
// purpose  : loads the list of cookies into the cookie list treeview
function loadCookies()
{
  //  get cookies into an array
  list            = cookieviewer.GetCookieValue();
  BREAK           = list[0];
  cookieList      = list.split(BREAK);
  CreateCookieList();   // builds an object array from cookiestream
  for(i = 0; i < cookies.length; i++)
  {
    var domain = cookies[i].domain;
    if(domain.charAt(0) == ".")   // get rid of the ugly dot on the start of some domains
      domain = domain.substring(1,domain.length);
    AddItem("cookielist", [domain,cookies[i].name], "tree_", cookies[i].number); 
  }
  if (cookies.length == 0) {
    document.getElementById("removeAllCookies").setAttribute("disabled","true");
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

  if( !cookie || cookie.getAttribute("id").indexOf("tree_") == -1)
    return false;
  var idx = parseInt(cookie.getAttribute("id").substring(5,cookie.getAttribute("id").length));
  for (x=0; x<cookies.length; x++) {
    if (cookies[x].number == idx) {
      idx = x;
      break;
    }
  }
  var props = [cookies[idx].number, cookies[idx].name, cookies[idx].value, 
               cookies[idx].domaintype, cookies[idx].domain, cookies[idx].path,
               cookies[idx].secure, cookies[idx].expire];

  rows =
    [null,"ifl_name","ifl_value","ifl_domaintype","ifl_domain","ifl_path","ifl_secure","ifl_expires"];
  for(i = 1; i < props.length; i++)
  {
    if(i == 3) {
      var dtypecell = document.getElementById("ifl_domaintype");
      dtypecell.setAttribute("value", cookies[idx].domaintype+":");
      continue;
    }
    var field = document.getElementById(rows[i]);
    var content = props[i];
    var value = ( !selItemsMax ) ? content : "";  // multiple selections clear fields.
    field.setAttribute("value", value);
    if(rows[i] == "ifl_expires") break;
  }
}

// function : <CookieViewer.js>::DeleteCookieSelected();
// purpose  : deletes all the cookies that are selected
function DeleteCookieSelected() {
  // delete selected item
  deleted_cookies_count += document.getElementById("cookietree").selectedItems.length;
  gone_c += DeleteItemSelected("cookietree", "tree_", "cookielist");
  // set fields
  rows = ["ifl_name","ifl_value","ifl_domain","ifl_path","ifl_secure","ifl_expires"];
  for(k = 0; k < rows.length; k++) 
  {
    var row = document.getElementById(rows[k]);
    row.setAttribute("value","");
  }
  if( !document.getElementById("cookietree").selectedItems.length ) {
    if( !document.getElementById("removeCookies").disabled ) {
      document.getElementById("removeCookies").setAttribute("disabled", "true")
    }
  }
  if (deleted_cookies_count >= cookies.length) {
    document.getElementById("removeAllCookies").setAttribute("disabled","true");
  }
}

// function : <CookieViewer.js>::DeleteAllCookies();
// purpose  : deletes all the cookies
function DeleteAllCookies() {
  // delete selected item
  gone_c += DeleteAllItems(cookies.length, "tree_", "cookielist");
  // set fields
  rows = ["ifl_name","ifl_value","ifl_domain","ifl_path","ifl_secure","ifl_expires"];
  for(k = 0; k < rows.length; k++) 
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

// function : <CookieViewer.js>::CreatePermissionList();
// purpose  : creates an array of permission objects from the permission stream
function CreatePermissionList()
{
  count = 0;
  for(i = 1; i < permissionList.length; i+=2)
  {
    permissions[count] = new Permission();
    permissions[count].number     = permissionList[i];
    permStr                       = permissionList[i+1];
    permissions[count].type       = permStr.substring(0,1);
    permissions[count].domain     = permStr.substring(1,permStr.length);
    count++;
  }  
}

// function : <CookieViewer.js>::Permission();
// purpose  : an home-brewed object that represents a individual permission in the stream
function Permission(number,type,domain)
{
  this.number       = (number) ? number : null;
  this.type         = (type) ? type : null;
  this.domain       = (domain) ? domain : null;
}

// function : <CookieViewer.js>::loadPermissions();
// purpose  : loads the list of permissions into the permission list treeview
function loadPermissions()
{
  // get permissions into an array
  list            = cookieviewer.GetPermissionValue(0);
  BREAK           = list[0];
  permissionList  = list.split(BREAK);
  CreatePermissionList();   // builds an object array from permissionstream
  for(i = 0; i < permissions.length; i++)
  {
    var domain = permissions[i].domain;
    if(domain.charAt(0) == ".")   // get rid of the ugly dot on the start of some domains
      domain = domain.substring(1,domain.length);
    if(permissions[i].type == "+")
      contentStr = bundle.GetStringFromName("can");
    else if(permissions[i].type == "-")
      contentStr = bundle.GetStringFromName("cannot");    
    AddItem("permissionslist",[domain,contentStr],"permtree_",permissions[i].number)
  }
  if (permissions.length == 0) {
    document.getElementById("removeAllPermissions").setAttribute("disabled","true");
  }
}

function ViewPermissionSelected()
{
  var permissiontree = document.getElementById("permissionstree");
  if( permissiontree.selectedItems.length )
    document.getElementById("removePermissions").removeAttribute("disabled","true");
}

function DeletePermissionSelected()
{
  deleted_permissions_count += document.getElementById("permissionstree").selectedItems.length;
  gone_p += DeleteItemSelected('permissionstree', 'permtree_', 'permissionslist');
  if( !document.getElementById("permissionstree").selectedItems.length ) {
    if( !document.getElementById("removePermissions").disabled ) {
      document.getElementById("removePermissions").setAttribute("disabled", "true")
    }
  }
  if (deleted_permissions_count >= permissions.length) {
    document.getElementById("removeAllPermissions").setAttribute("disabled","true");
  }
}

function DeleteAllPermissions() {
  // delete selected item
  gone_p += DeleteAllItems(permissions.length, "permtree_", "permissionslist");
  if( !document.getElementById("removePermissions").disabled ) {
    document.getElementById("removePermissions").setAttribute("disabled", "true")
  }
  document.getElementById("removeAllPermissions").setAttribute("disabled","true");
}

/*** =================== IMAGES CODE =================== ***/

// function : <CookieViewer.js>::CreateImageList();
// purpose  : creates an array of image objects from the image stream
function CreateImageList()
{
  count = 0;
  for(i = 1; i < imageList.length; i+=2)
  {
    images[count] = new Image();
    images[count].number     = imageList[i];
    imgStr                   = imageList[i+1];
    images[count].type       = imgStr.substring(0,1);
    images[count].domain     = imgStr.substring(1,imgStr.length);
    count++;
  }  
}

// function : <CookieViewer.js>::Image();
// purpose  : an home-brewed object that represents a individual image in the stream
function Image(number,type,domain)
{
  this.number       = (number) ? number : null;
  this.type         = (type) ? type : null;
  this.domain       = (domain) ? domain : null;
}

// function : <CookieViewer.js>::loadImages();
// purpose  : loads the list of images into the image list treeview
function loadImages()
{
  // get images into an array
  list = cookieviewer.GetPermissionValue(1);
  BREAK = list[0];
  imageList = list.split(BREAK);
  CreateImageList();   // builds an object array from imagestream
  for(i = 0; i < images.length; i++)
  {
    var domain = images[i].domain;
    if(images[i].type == "+")
      contentStr = bundle.GetStringFromName("canImages");
    else if(images[i].type == "-")
      contentStr = bundle.GetStringFromName("cannotImages");    
    AddItem("imageslist",[domain,contentStr],"imgtree_",images[i].number)
  }
  if (images.length == 0) {
    document.getElementById("removeAllImages").setAttribute("disabled","true");
  }
}

function ViewImageSelected()
{
  var imagetree = document.getElementById("imagestree");
  if( imagetree.selectedItems.length )
    document.getElementById("removeImages").removeAttribute("disabled","true");
}

function DeleteImageSelected()
{
  deleted_images_count += document.getElementById("imagestree").selectedItems.length;
  gone_i += DeleteItemSelected('imagestree', 'imgtree_', 'imageslist');
  if( !document.getElementById("imagestree").selectedItems.length ) {
    if( !document.getElementById("removeImages").disabled ) {
      document.getElementById("removeImages").setAttribute("disabled", "true")
    }
  }
  if (deleted_images_count >= images.length) {
    document.getElementById("removeAllImages").setAttribute("disabled","true");
  }
}

function DeleteAllImages() {
  // delete selected item
  gone_i += DeleteAllItems(images.length, "imgtree_", "imageslist");
  if( !document.getElementById("removeImages").disabled ) {
    document.getElementById("removeImages").setAttribute("disabled", "true")
  }
  document.getElementById("removeAllImages").setAttribute("disabled","true");
}

/*** =================== GENERAL CODE =================== ***/

// function : <CookieViewer.js>::doOKButton();
// purpose  : saves the changed settings and closes the dialog.
function onOK(){
  var result = "|goneC|" + gone_c + "|goneP|" + gone_p  + "|goneI|" + gone_i + 
               "|block|" + document.getElementById("checkbox").checked +"|";
  cookieviewer.SetValue(result, window);
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
    cell.setAttribute("value", cells[i])
    row.appendChild(cell);
  }
  item.appendChild(row);
  item.setAttribute("id",prefix + idfier);
  kids.appendChild(item);
}

// function : <CookieViewer.js>::DeleteItemSelected();
// purpose  : deletes all the signons that are selected
function DeleteItemSelected(tree, prefix, kids) {
  var delnarray = [];
  var rv = "";
  var cookietree = document.getElementById(tree);
  selitems = cookietree.selectedItems;
  for(var i = 0; i < selitems.length; i++) 
  { 
    delnarray[i] = document.getElementById(selitems[i].getAttribute("id"));
    var itemid = parseInt(selitems[i].getAttribute("id").substring(prefix.length,selitems[i].getAttribute("id").length));
    rv += (itemid + ",");
  }
  for(var i = 0; i < delnarray.length; i++) 
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
