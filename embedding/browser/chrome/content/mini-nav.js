/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

var appCore = null;
var locationFld = null;
var commandHandler = null;


function nsCommandHandler()
{
}

nsCommandHandler.prototype = 
{
  QueryInterface : function(iid)
    {
      if (iid.equals(Components.interfaces.nsICommandHandler))
      {
        return this;
      }
      throw Components.results.NS_NOINTERFACE;
    },

    exec : function(command, params)
      {
      }
    query : function(command, params, result)
      {
        result = "";
      }
}

//

function nsXULBrowserWindow()
{
}

nsXULBrowserWindow.prototype = 
{
  QueryInterface : function(iid)
    {
    if(iid.equals(Components.interfaces.nsIXULBrowserWindow))
      return this;
    throw Components.results.NS_NOINTERFACE;
    },
  setJSStatus : function(status)
    {
    },
  setJSDefaultStatus : function(status)
    {
    },
  setDefaultStatus : function(status)
    {
    },
  setOverLink : function(link)
    {
    },
  onProgress : function (channel, current, max)
    {
    },
  onStatusChange : function(channel, status)
    {
    },
  onLocationChange : function(location)
    {
      if(!locationFld)
        locationFld = document.getElementById("urlbar");

      // We should probably not do this if the value has changed since the user 
      // searched
      locationFld.setAttribute("value", location);
    }
}


function Startup()
{
  window.XULBrowserWindow = new nsXULBrowserWindow();

  // Create the browser instance component.
  createBrowserInstance();


  window._content.appCore= appCore;
  if (appCore == null) {
    // Give up.
    window.close();
  }
  // Initialize browser instance..
  appCore.setWebShellWindow(window);

  // create the embedding command handler
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  var commandHandlerInit = Components
      .classes["component://netscape/embedding/browser/nsCommandHandler"]
      .createInstance(Components.interfaces.nsICommandHandlerInit);

  // Attach it to the window
  commandHandlerInit.window = window;
  commandHandler = commandHandlerInit.QueryInterface(Components.interfaces.nsICommandHandler);

  gURLBar = document.getElementById("urlbar");
}

function Shutdown()
{
  // Close the app core.
  if ( appCore )
    appCore.close();
}

function createBrowserInstance()
{
  appCore = Components
      .classes[ "component://netscape/appshell/component/browser/instance" ]
      .createInstance( Components.interfaces.nsIBrowserInstance );
  if ( !appCore ) {
      alert( "Error creating browser instance\n" );
  }
}

function CHExecTest()
{
  if (commandHandler != null)
  {
    commandHandler.exec("hello", "xxx");
  }
}

function CHQueryTest()
{
  if (commandHandler != null)
  {
    var result = commandHandler.query("hello", "xxx");
  }
}

function InitContextMenu(xulMenu)
{
  // Back determined by canGoBack broadcaster.
  InitMenuItemAttrFromNode( "context-back", "disabled", "canGoBack" );

  // Forward determined by canGoForward broadcaster.
  InitMenuItemAttrFromNode( "context-forward", "disabled", "canGoForward" );
}

function InitMenuItemAttrFromNode( item_id, attr, other_id )
{
  var elem = document.getElementById( other_id );
  if ( elem && elem.getAttribute( attr ) == "true" ) {
    SetMenuItemAttr( item_id, attr, "true" );
  } else {
    SetMenuItemAttr( item_id, attr, null );
  }
}

function SetMenuItemAttr( id, attr, val )
{
  var elem = document.getElementById( id );
  if ( elem ) {
    if ( val == null ) {
      // null indicates attr should be removed.
      elem.removeAttribute( attr );
    } else {
      // Set attr=val.
      elem.setAttribute( attr, val );
    }
  }
}


function BrowserLoadURL()
{
  try {
    appCore.loadUrl(gURLBar.value);
  }
  catch(e) {
  }
}

function BrowserBack()
{
  appCore.back();
}

function BrowserForward()
{
  appCore.forward();
}

function BrowserStop()
{
  appCore.stop();
}

function BrowserReload()
{
  appCore.reload(0);
}

