/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Original Author: 
 *   Ben Goodger <ben@netscape.com>
 *
 * Contributor(s): 
 */
 
 
const _DEBUG = true;
function _dd(aString)
{
  if (_DEBUG)
    dump("*** " + aString + "\n");
}

const kMenuHeight = 130;
const kPropertiesWidth = 170;


function vx_Startup()
{
  _dd("vx_Startup");
  window.moveTo(0,0);
  window.outerWidth = screen.availWidth;
  window.outerHeight = kMenuHeight;
  
  // initialise commands
  controllers.insertControllerAt(0, defaultController);
 
  // load a scratch document
  vx_LoadForm("chrome://vixen/content/vfdScratch.xul");
  
  var url = Components.classes["component://netscape/network/standard-url"].createInstance();
  if (url) url = url.QueryInterface(Components.interfaces.nsIURI);
  url.spec = "chrome://vixen/skin/vfdScratch.css";
  var chromeRegistry = Components.classes["component://netscape/chrome/chrome-registry"].getService();
  chromeRegistry = chromeRegistry.QueryInterface( Components.interfaces.nsIChromeRegistry );
  var url2 = chromeRegistry.convertChromeURL(url);
  dump(url2);
}

function vx_Shutdown()
{
  _dd("vx_Shutdown");
  
  const WM_PROGID = "component://netscape/rdf/datasource?name=window-mediator";
  var wm = nsJSComponentManager.getService(WM_PROGID, "nsIWindowMediator");
  var windows = wm.getXULWindowEnumerator("xuledit:document");
  while (windows.hasMoreElements()) {
    var currWindow = windows.getNext();
    currWindow.close();
  }
}

function vx_LoadForm(aURL)
{
  hwnd = openDialog(aURL, "", "chrome,dialog=no,resizable");
  hwnd.moveTo(kPropertiesWidth + 5, kMenuHeight + 5);
  hwnd.outerWidth = screen.availWidth - (kPropertiesWidth * 2) - 5;
  hwnd.outerHeight = screen.availHeight - kMenuHeight - 20;
}

var defaultController = {
  supportsCommand: function(command)
  {
    switch (command) {
    
    case "cmd_insert_button":
    case "cmd_insert_toolbarbutton":
    case "cmd_insert_menubutton":

    case "cmd_insert_toplevel_menu":
    case "cmd_insert_menu":
    case "cmd_insert_menuseparator":
    case "cmd_insert_menulist":
    case "cmd_insert_combobox":
    
    case "cmd_insert_textfield":
    case "cmd_insert_textarea":
    case "cmd_insert_rdf_editor":
    
    case "cmd_insert_static":
    case "cmd_insert_wrapping":
    case "cmd_insert_image":
    case "cmd_insert_browser":
    
    case "cmd_insert_box":
    case "cmd_insert_grid":
    case "cmd_insert_grid_row":
    case "cmd_insert_grid_col":
    case "cmd_insert_grid_spring":
    case "cmd_insert_splitter":
      return true;
      
    default:
      return false;
    }
  },
  
  isCommandEnabled: function(command)
  {
    switch (command) {    
    case "cmd_insert_button":
    case "cmd_insert_toolbarbutton":
    case "cmd_insert_menubutton":

    case "cmd_insert_toplevel_menu":
    case "cmd_insert_menu":
    case "cmd_insert_menuseparator":
    case "cmd_insert_menulist":
    case "cmd_insert_combobox":
    
    case "cmd_insert_textfield":
    case "cmd_insert_textarea":
    case "cmd_insert_rdf_editor":
    
    case "cmd_insert_static":
    case "cmd_insert_wrapping":
    case "cmd_insert_image":
    case "cmd_insert_browser":
    
    case "cmd_insert_box":
    case "cmd_insert_grid":
    case "cmd_insert_grid_row":
    case "cmd_insert_grid_col":
    case "cmd_insert_grid_spring":
    case "cmd_insert_splitter":
      return true;
    
    default:
      return false;
    }
  },
  
  doCommand: function(command)
  {
    switch (command)
    {
    
    case "cmd_insert_button":
      nsVFD.insertButtonElement("button");
      return true;
    case "cmd_insert_toolbarbutton":
    case "cmd_insert_menubutton":

    case "cmd_insert_toplevel_menu":
    case "cmd_insert_menu":
    case "cmd_insert_menuseparator":
    case "cmd_insert_menulist":
    case "cmd_insert_combobox":
    
    case "cmd_insert_textfield":
    case "cmd_insert_textarea":
    case "cmd_insert_rdf_editor":
    
    case "cmd_insert_static":
    case "cmd_insert_wrapping":
    case "cmd_insert_image":
    case "cmd_insert_browser":
    
    case "cmd_insert_box":
    case "cmd_insert_grid":
    case "cmd_insert_grid_row":
    case "cmd_insert_grid_col":
    case "cmd_insert_grid_spring":
    case "cmd_insert_splitter":
      return true;
      
    default:
      return false;
    }
  },
  
  onEvent: function(event)
  {
  }
}
