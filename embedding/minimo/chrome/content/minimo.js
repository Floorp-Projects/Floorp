/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const nsIWebNavigation = Components.interfaces.nsIWebNavigation;
const nsIWebProgressListener = Components.interfaces.nsIWebProgressListener;

var gURLBar = null;

function nsBrowserStatusHandler()
{
}

nsBrowserStatusHandler.prototype = 
{
  QueryInterface : function(aIID)
  {
    if (aIID.equals(Components.interfaces.nsIWebProgressListener) ||
        aIID.equals(Components.interfaces.nsISupportsWeakReference) ||
        aIID.equals(Components.interfaces.nsISupports))
    {
      return this;
    }
    throw Components.results.NS_NOINTERFACE;
  },

  init : function()
  {
    this.urlBar           = document.getElementById("urlbar");
    this.statusbarText    = document.getElementById("statusbar-text");
    this.stopreloadButton = document.getElementById("reload-stop-button");
    this.statusbar        = document.getElementById("statusbar");
    this.transferCount=0;

  },

  destroy : function()
  {
    this.urlBar = null;
    this.statusbarText = null;
    this.stopreloadButton = null;
    this.statusbar = null;
  },

  onStateChange : function(aWebProgress, aRequest, aStateFlags, aStatus)
  {

    if(aStateFlags & nsIWebProgressListener.STATE_TRANSFERRING) { 
      this.transferCount+=5;
      document.styleSheets[1].cssRules[0].style.backgroundPosition=this.transferCount+"px 100%";

    }

    if (aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK)
    {

      if (aStateFlags & nsIWebProgressListener.STATE_START)
      {
        this.transferCount=0;
        document.styleSheets[1].cssRules[0].style.backgroundImage="url(chrome://minimo/skin/transfer.gif)";

        this.stopreloadButton.image = "chrome://minimo/skin/stop.gif";
        this.stopreloadButton.onClick = "BrowserStop()";
        this.statusbar.hidden = false;
        return;
      }
      
      if (aStateFlags & nsIWebProgressListener.STATE_STOP)
      {
        document.styleSheets[1].cssRules[0].style.backgroundPosition="1000px 100%";
      
        this.stopreloadButton.image = "chrome://minimo/skin/reload.gif";
        this.stopreloadButton.onClick = "BrowserReload()";
        
        this.statusbar.hidden = true;
        this.statusbarText.label = "";
        return;
      }
  


      return;
    }

    if (aStateFlags & nsIWebProgressListener.STATE_IS_DOCUMENT)
    { 
      if (aStateFlags & nsIWebProgressListener.STATE_START)
      {
        return;
      }
      
      if (aStateFlags & nsIWebProgressListener.STATE_STOP)
      {
        return;
      }

      return;
    }
  },

  onProgressChange : function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress)
  {
    //alert("aWebProgress="+aWebProgress+"aRequest.name: "+aRequest.name+"aCurSelfProgress:"+aCurSelfProgress);
  },


  onLocationChange : function(aWebProgress, aRequest, aLocation)
  {
    domWindow = aWebProgress.DOMWindow;
    // Update urlbar only if there was a load on the root docshell
    if (domWindow == domWindow.top) {
      this.urlBar.value = aLocation.spec;
    }
  },

  onStatusChange : function(aWebProgress, aRequest, aStatus, aMessage)
  {
    this.statusbarText.label = aMessage;
  },

  onSecurityChange : function(aWebProgress, aRequest, aState)
  {
  }
}

var gBrowserStatusHandler;
function MiniNavStartup()
{
  try {

    gBrowserStatusHandler = new nsBrowserStatusHandler();
    gBrowserStatusHandler.init();

    var interfaceRequestor = getBrowser().docShell.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
    var webProgress = interfaceRequestor.getInterface(Components.interfaces.nsIWebProgress);
    webProgress.addProgressListener(gBrowserStatusHandler, Components.interfaces.nsIWebProgress.NOTIFY_ALL);

    var webNavigation = getWebNavigation();

    webNavigation.sessionHistory = Components.classes["@mozilla.org/browser/shistory;1"]
                                             .createInstance(Components.interfaces.nsISHistory);

  } 
  catch (e) 
  {
    alert(e);
  }


  //  try {
  //    getBrowser().markupDocumentViewer.textZoom = .75;
  //  }
  //  catch (e) {}

  gURLBar = document.getElementById("urlbar");

  loadURI("http://www.google.com");


 //alert(document.styleSheets[0].cssRules[2].href);


}

function MiniNavShutdown()
{
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

function loadURI(uri)
{
  getWebNavigation().loadURI(uri, nsIWebNavigation.LOAD_FLAGS_NONE, null, null, null);
}



function BrowserLoadURL()
{
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

////
/// marcio 
//
function urlBarIdentity() {
    if(0) {
    }
    
}

function addTab() {
    getBrowser().addTab("about:blank");
}
