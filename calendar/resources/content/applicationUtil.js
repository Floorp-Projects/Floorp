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

const nsIWindowMediator = Components.interfaces.nsIWindowMediator;

function toOpenWindowByType( inType, uri )
{
        var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService();

        var     windowManagerInterface = windowManager.QueryInterface(nsIWindowMediator);

        var topWindow = windowManagerInterface.getMostRecentWindow( inType );
        
        if ( topWindow )
                topWindow.focus();
        else
                window.open(uri, "_blank", "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar");
}

function toBrowser()
{
    toOpenWindowByType("navigator:browser", "");
}

function toJavaScriptConsole()
{
    toOpenWindowByType("global:console", "chrome://global/content/console.xul");
}

function toMessengerWindow()
{
  toOpenWindowByType("mail:3pane", "chrome://messenger/content/messenger.xul");
}
    
function toAddressBook() 
{
  toOpenWindowByType("mail:addressbook", "chrome://messenger/content/addressbook/addressbook.xul");
}

function launchBrowser( UrlToGoTo )
{
   if( navigator.vendor != "Thunderbird") {
     var navWindow;

     // if not, get the most recently used browser window
       try {
         var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
           .getService(Components.interfaces.nsIWindowMediator);
         navWindow = wm.getMostRecentWindow("navigator:browser");
       }
       catch (ex) { }
     
     if (navWindow) {
       if ("delayedOpenTab" in navWindow)
         navWindow.delayedOpenTab(UrlToGoTo);
       else if ("loadURI" in navWindow)
         navWindow.loadURI(UrlToGoTo);
       else
         navWindow.content.location.href = UrlToGoTo;
     }
     // if all else fails, open a new window 
     else {

       var ass = Components.classes["@mozilla.org/appshell/appShellService;1"].getService(Components.interfaces.nsIAppShellService);
       w = ass.hiddenDOMWindow;

       w.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", UrlToGoTo );
     }
   } 
   else
     {
       try {
            var messenger = Components.classes["@mozilla.org/messenger;1"].createInstance();
            messenger = messenger.QueryInterface(Components.interfaces.nsIMessenger);
            messenger.launchExternalURL(UrlToGoTo);  
       } catch (ex) {}
     }
   //window.open(UrlToGoTo, "_blank", "chrome,menubar,toolbar,resizable,dialog=no");
   //window.open( UrlToGoTo, "calendar-opened-window" );
}

function goToggleToolbar( id, elementID )
{
  var toolbar = document.getElementById(id);
  var element = document.getElementById(elementID);
  if (toolbar) {
    var isHidden = toolbar.hidden;
    toolbar.hidden = !isHidden;
    document.persist(id, 'hidden');
    if (element) {
      element.setAttribute("checked", isHidden ? "true" : "false");
      document.persist(elementID, 'checked');
    }
  }
}

function openExtensions(aOpenMode)
{
  const EMTYPE = "Extension:Manager";
  
  var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                     .getService(Components.interfaces.nsIWindowMediator);
  var needToOpen = true;
  var windowType = EMTYPE + "-" + aOpenMode;
  var windows = wm.getEnumerator(windowType);
  while (windows.hasMoreElements()) {
    var theEM = windows.getNext().QueryInterface(Components.interfaces.nsIDOMWindowInternal);
    if (theEM.document.documentElement.getAttribute("windowtype") == windowType) {
      theEM.focus();
      needToOpen = false;
      break;
    }
  }

  if (needToOpen) {
    const EMURL = "chrome://mozapps/content/extensions/extensions.xul?type=" + aOpenMode;
    const EMFEATURES = "chrome,dialog=no,resizable";
    window.openDialog(EMURL, "", EMFEATURES);
  }
}

function showElement( elementId )
{
  try {
    dump("showElement: showing "+elementId+"\n");
    document.getElementById( elementId ).removeAttribute( "hidden" );
  }
  catch (e) {
    dump("showElement: Couldn't remove hidden attribute from "+elementId+"\n");
  }
}

function hideElement( elementId )
{
  try {
    dump("hideElement: hiding "+elementId+"\n");
    document.getElementById( elementId ).setAttribute( "hidden", "true" );
  }
  catch (e) {
    dump("hideElement: Couldn't set hidden attribute on "+elementId+"\n");
  }
}
