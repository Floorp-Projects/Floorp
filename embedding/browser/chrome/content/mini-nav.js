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

const nsIWebNavigation = Components.interfaces.nsIWebNavigation;

var commandHandler = null;
var gURLBar = null;

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
      },
    query : function(command, params, result)
      {
        result = "";
      }
}

//

function nsBrowserStatusHandler()
{
  this.init();
}

nsBrowserStatusHandler.prototype = 
{
  QueryInterface : function(aIID)
  {
    if (aIID.equals(Components.interfaces.nsIWebProgressListener) ||
        aIID.equals(Components.interfaces.nsISupportsWeakReference))
      return this;
    throw Components.results.NS_NOINTERFACE;
  },

  init : function()
  {
    this.urlBar = document.getElementById("urlbar");
  },

  destroy : function()
  {
    this.urlBar = null;
  },

  onStateChange : function(aWebProgress, aRequest, aStateFlags, aStatus)
  {
  },

  onProgressChange : function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress)
  {
  },

  onLocationChange : function(aWebProgress, aRequest, aLocation)
  {
    domWindow = aWebProgress.DOMWindow;
    // Update urlbar only if there was a load on the root docshell
    if (domWindow == domWindow.top) {
      this.urlBar.value = location;
    }
  },

  onStatusChange : function(aWebProgress, aRequest, aStatus, aMessage)
  {
  },

  onSecurityChange : function(aWebProgress, aRequest, aState)
  {
  }
}

var gBrowserStatusHandler;
function MiniNavStartup()
{
  dump("*** MiniNavStartup\n");

  try {
    gBrowserStatusHandler = new nsBrowserStatusHandler();
    var webNavigation = getWebNavigation();
    webNavigation.sessionHistory = Components.classes["@mozilla.org/browser/shistory;1"]
                                             .createInstance(Components.interfaces.nsISHistory);

    var interfaceRequestor = getBrowser().docShell.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
    var webProgress = interfaceRequestor.getInterface(Components.interfaces.nsIWebProgress);
    webProgress.addProgressListener(gBrowserStatusHandler);
  } catch (e) {
    alert("Error opening a mini-nav window"); 
    dump(e+"\n");
    window.close();
    return;
  }

  // create the embedding command handler
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  var commandHandlerInit = Components
      .classes["@mozilla.org/embedding/browser/nsCommandHandler;1"]
      .createInstance(Components.interfaces.nsICommandHandlerInit);

  // Attach it to the window
  commandHandlerInit.window = window;
  commandHandler = commandHandlerInit.QueryInterface(Components.interfaces.nsICommandHandler);

  gURLBar = document.getElementById("urlbar");
  dump("gURLBar " + gURLBar + "\n");
}

function MiniNavShutdown()
{
  dump("*** MiniNavShutdown\n");
  if (gBrowserStatusHandler)
    gBrowserStatusHandler.destroy();
}

function getBrowser()
{
  return document.getElementById("content");
}

function getWebNavigation()
{
  return getBrowser().webNavigation;
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

function loadURI(uri)
{
  getWebNavigation().loadURI(uri, nsIWebNavigation.LOAD_FLAGS_NONE);
}

function BrowserLoadURL()
{
  dump("browserloadurl: " + gURLBar.value + '\n');
  try {
    loadURI(gURLBar.value);
  }
  catch(e) {
  }
}

function BrowserBack()
{
  getWebNavigation().goBack();
}

function BrowserForward()
{
  getWebNavigation().goForward();
}

function BrowserStop()
{
  getWebNavigation().stop(nsIWebNavigation.STOP_ALL);
}

function BrowserReload()
{
  getWebNavigation().reload(nsIWebNavigation.LOAD_FLAGS_NONE);
}

