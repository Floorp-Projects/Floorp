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
var deleted_cookies       = [];
var deleted_permissions   = [];
// for dealing with the interface:
var gone_c                = "";
var gone_p                = "";
// string bundle
var bundle                = null;
// CHANGE THIS WHEN MOVING FILES - strings localization file!
var JS_STRINGS_FILE       = "chrome://wallet/locale/CookieViewer.properties";
    
// function : <CookieViewer.js>::Startup();
// purpose  : initialises the cookie viewer dialog
function Startup()
{
  // xpconnect to cookieviewer interface
  cookieviewer = Components.classes["component://netscape/cookieviewer/cookieviewer-world"].createInstance();
  cookieviewer = cookieviewer.QueryInterface(Components.interfaces.nsICookieViewer);
  // intialise string bundle for 
  bundle = srGetStrBundle(JS_STRINGS_FILE);
  
  loadCookies();
  loadPermissions();
  doSetOKCancel(onOK, null);
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
  this.number       = (arguments.length) ? number     : null;
  this.name         = (arguments.length) ? name       : null;
  this.value        = (arguments.length) ? value      : null;
  this.domaintype   = (arguments.length) ? domaintype : null;
  this.domain       = (arguments.length) ? domain     : null;
  this.path         = (arguments.length) ? path       : null;
  this.secure       = (arguments.length) ? secure     : null;
  this.expire       = (arguments.length) ? expire     : null;
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
}

// function : <CookieViewer.js>::ViewSelectedCookie();
// purpose  : displays information about the selected cookie in the info fieldset
function ViewCookieSelected(node) 
{
  var cookietree = document.getElementById("cookietree");
  if(cookietree.nodeName != "tree")
    return false;
  if(cookietree.selectedItems.length > 1)
    return false;
  cookie = node;
  if(cookie.getAttribute("id").indexOf("tree_") == -1)
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
      if(dtypecell.hasChildNodes()) {
        dtypecell.removeChild(dtypecell.lastChild);
      }
      var content = document.createTextNode(cookies[idx].domaintype+":");
      dtypecell.appendChild(content);
      continue;
    }
    var field = document.getElementById(rows[i]);
    var content = props[i];
    field.value = content;
    if(rows[i] == "ifl_expires") break;
  }
}

// function : <CookieViewer.js>::DeleteCookieSelected();
// purpose  : deletes all the cookies that are selected
function DeleteCookieSelected() {
  // delete selected item
  gone_c += DeleteItemSelected("cookietree", "tree_", "cookielist");
  // set fields
  rows = ["ifl_name","ifl_value","ifl_domain","ifl_path","ifl_secure","ifl_expires"];
  for(k = 0; k < rows.length; k++) 
  {
    var row = document.getElementById(rows[k]);
    row.setAttribute("value","");
  }
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
  this.number       = (arguments.length) ? number     : null;
  this.type         = (arguments.length) ? type       : null;
  this.domain       = (arguments.length) ? domain     : null;
}

// function : <CookieViewer.js>::loadPermissions();
// purpose  : loads the list of permissions into the permission list treeview
function loadPermissions()
{
  // get permissions into an array
  list            = cookieviewer.GetPermissionValue();
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
}

/*** =================== GENERAL CODE =================== ***/

// function : <CookieViewer.js>::doOKButton();
// purpose  : saves the changed settings and closes the dialog.
function onOK(){
  var result = "|goneC|" + gone_c + "|goneP|" + gone_p + "|";
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
    var text = document.createTextNode(cells[i]);
    cell.appendChild(text);
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