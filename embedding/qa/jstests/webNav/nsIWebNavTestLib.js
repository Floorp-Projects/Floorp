/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 *   David Epstein <depstein@netscape.com>
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

// nsIWebNavTestLib.js is a JavaScript library containing methods for
// testing of nsIWebNavigation interface. 

// ***************************************************************************
// webNavInit() creates and returns the session history object. 
function webNavInit(theWindow)
{
  //alert("In nsIWebNavTest::Init method");
  var webNav = null;
  try
  {       
    netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserAccess");
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");       
  
    var ifaceReq = theWindow.QueryInterface(Components.interfaces.nsIInterfaceRequestor);    
    if (!ifaceReq) {
       alert("Unable to get interface requestor object");
       return false;
    }
    webNav = ifaceReq.getInterface(Components.interfaces.nsIWebNavigation);
 
    if(!webNav) {
      alert("Unable to get WebNavigation");
      return false;
    }               
 
    return webNav;
  }
  catch(e) 
  {
    alert("Could not find Web Navigation component" + e);
    return false;
  }
}

// *************************************************************************
// testCanGoBack() returns a boolean to determine if goBack is possible.
// It accepts the web navigation object as the first input parameter.
// The second parameter is used to trigger goBack() if it's true.
function testCanGoBack(webNav, navEvent)
{
  if (!webNav) {
    alert("Didn't get web navigation object");
    return false;  
  }
  
  netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserAccess");
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  
  var back = webNav.canGoBack;  	// attribute canGoBack
  if (back == true && navEvent == 1)
  {
    dump("goBack value : " + back  + "\n");  
    setTimeout("testGoBack(webNav, 0)");
  }
  
  return back; 
}

// *************************************************************************
// testCanGoForward() returns a boolean to determine if goForward is possible.
// It accepts the web navigation object as the input parameter.
// The second parameter is used to trigger goBack() if it's true.
function testCanGoForward(webNav, navEvent)
{
  if (!webNav) {
    alert("Didn't get web navigation object");
    return false;  
  }
  
  netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserAccess");
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  
  var forward = webNav.canGoForward;   // attribute canGoForward
  if (forward == true && navEvent == 1)
  {
    dump("goForward value : " + forward  + "\n");
    setTimeout("testGoForward(webNav, 0)", 10000);
  }
  
  return forward;
}

// *************************************************************************
// testGoBack() navigates to the previous url in the session history.
// It accepts the web navigation object as the input parameter.
function testGoBack(webNav)
{
  dump("inside testGoBack()\n");

  if (!webNav) {
    alert("Didn't get web navigation object");
    return false;  
  }
  netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserAccess");
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
    
  webNav.goBack();  		// function goBack()
}

// *************************************************************************
// testGoForward() navigates to the next url in the session history.
// It accepts the web navigation object as the input parameter.
function testGoForward(webNav)
{
  dump("inside testGoForward()\n"); 
  netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserAccess");
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
   
  if (!webNav) {
    alert("Didn't get web navigation object");
    return false;  
  }
   
  webNav.goForward();  	// function goForward()    
}

// *************************************************************************
// testGotoIndex() navigates to the specified index of the session history.
// It accepts the web navigation object as the input parameter.
// The index specifying the sHistory entry is the 2nd parameter).
function testGotoIndex(webNav, index)
{
  dump("inside testGotoIndex()\n");  
  if (!webNav) {
    alert("Didn't get web navigation object");
    return false;  
  }
  netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserAccess");
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  
  webNav.gotoIndex(index);
}

// *************************************************************************

// testLoadURI() navigates to the designated url. It accepts 6 parameters:
// 1) the webNav object, 2) the uri (e.g. http://...), 3) flags for loading
// 4) a referring url, 5) Post data for request handling, 6) headers
function testLoadURI(webNav, uri, loadFlags, referrer, postData, headers)
{
 // alert("get inside of testLoadURI()");
  if (!webNav) {
    alert("Didn't get web navigation object");
    return false;  
  }
  netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserAccess");
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  
  webNav.loadURI(uri, loadFlags, referrer, postData, headers);
  getCurrURI = testCurrentURI(webNav);
  dump("the loaded uri = " + getCurrURI.spec + "\n");
}

// *************************************************************************
// testReload() reloads the current uri. It accepts the webNav object for
// the first parameter, the loaded Flags as the second one.
function testReload(webNav, loadFlags)
{
    dump("inside testReload()\n");
    if (!webNav) {
      alert("Didn't get web navigation object");
      return false;  
    }
  netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserAccess");
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
    
    webNav.reload(loadFlags);
}

// *************************************************************************
// testStop() stops the uri from being loaded. It accepts the webNav object
// for the first parameter, the loaded Flags as the second one. 
function testStop(webNav, stopFlags)
{
  if (!webNav) {
      alert("Didn't get web navigation object");
      return false;  
  }
  
  webNav.stop(stopFlags);
}

// *************************************************************************
// testDocument() returns the current DOM document for the web browser. It 
// accepts the webNav object for the parameter.
function testDocument(webNav)
{
  if (!webNav) {
    alert("Didn't get web navigation object");
    return false;  
  }
   // Get the document
   getDoc = webNav.document;
   
   return getDoc;
}

// *************************************************************************
// testCurrentURI() returns The currently loaded URI. It accepts the webNav
// object for the parameter.
function  testCurrentURI(webNav)
{
  if (!webNav) {
    alert("Didn't get web navigation object");
    return false;  
  }
  // Get the current URI
   getCurrURI = webNav.currentURI;
   
   return getCurrURI;
}

/*  not implemented yet  
// *************************************************************************
// testReferringURI() gets the referring URI. It accepts the webNav
// object for the parameter.
function testReferringURI(webNav)
{
  if (!webNav) {
    alert("Didn't get web navigation object");
    return false;
  } 
    // Get the refering URI
  getRefURI = webNav.referringURI;
 // alert("The referring uri = " + getRefURI.spec);

  return getRefURI;     
} 
*/

// *************************************************************************
// testSessionHistory() gets the session history. It accepts the webNav
// object for the parameter.
function testSessionHistory(webNav)
{
  if (!webNav) {
    alert("Didn't get web navigation object");
    return null;
  }
   // Get the session history 
  getSHistory = webNav.sessionHistory; 
  
  return getSHistory;
}

// ****************************************************************
// Supplementary test functions

