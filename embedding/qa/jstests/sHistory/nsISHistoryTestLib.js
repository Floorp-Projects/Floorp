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
 *   Radha Kulkarni <radha@netscape.com>
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

// nsISHistoryTestLib.js is a JavaScript library containing methods for
// testing of nsISHistory (session history) and nsIHistoryEntry interfaces. 
// See nsISHistoryTestCase1.html (located in the same folder) for examples 
// of how to use these methods.

// ***************************************************************************
// sHistoryInit() creates and returns the session history object. 
function sHistoryInit(theWindow)
{
  var sHistory = null;
  var ifaceReq;
  var webNav;
  
  try
  {       
    netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserAccess");
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");       
  
    try {
       ifaceReq = theWindow.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
    }
    catch(e){
       alert("Unable to get interface requestor object: " + e);
       return null;
    }
    try {
       webNav = ifaceReq.getInterface(Components.interfaces.nsIWebNavigation);
    }
    catch(e) {
      alert("Unable to get WebNavigation: " + e);
      return null;
    }               
    try {
       sHistory = webNav.sessionHistory;
    }
    catch(e) {
      alert("Didn't get SessionHistory object: " + e);
      return null;
    } 
    return sHistory;
  }
  catch(e) 
  {
    alert("Could not find Session History component: " + e);
    return null;
  }
}

// *************************************************************************
// testHistoryCount() returns the number of entries in the session history.
// It accepts the session history object as the input parameter.
function testHistoryCount(sHistory)
{
  if (!sHistory) {
    alert("Didn't get SessionHistory object");
    return 0;
  }
  var cnt = sHistory.count;     // GetCount() 
  return cnt; 
}

// *************************************************************************
// testHistoryIndex() returns the current session history index.
// returns negative one if the history object is null.
// It accepts the session history object as the input parameter.
// In your test script, 
function testHistoryIndex(sHistory)
{
  if (!sHistory) {
    alert("Didn't get SessionHistory object");
    return -1;
  }
  var shIndex = sHistory.index;   // GetIndex()
  return shIndex;
}

// *************************************************************************
// testMaxLength() returns the maximum number of session history entries.
// It accepts the session history object as the input parameter.
function testMaxLength(sHistory)
{
  if (!sHistory) {
    alert("Didn't get SessionHistory object");
    return 0;
  }
  var maxLen = sHistory.maxLength;  // GetMaxLength
  return maxLen;
}

// *************************************************************************
// testGetEntryAtIndex() returns a referenced entry of the session history.
// index. It accepts the session history object as the 1st input parameter,
// a counter to track entries for the 2nd parameter, and a modify index 
// parameter for the 3rd one (which if 'true' allows the index entry to be
// modified).
function testGetEntryAtIndex(sHistory, cnt, modIndex)
{
  if (!sHistory) {
    alert("Didn't get SessionHistory object");
    return null;
  }
  var entry = sHistory.getEntryAtIndex(cnt, modIndex);

  return entry;
}

// *************************************************************************
function  testSimpleEnum(sHistory)
// testSimpleEnum() returns an enumerated list of all the session history
// entries. It accepts the session history object as the input parameter
{
  if (!sHistory) {
    alert("Didn't get SessionHistory object");
    return null;
  }
   var simpleEnum = sHistory.SHistoryEnumerator;
   
   return simpleEnum;
}

// *************************************************************************
// testHistoryEntry() obtains the URI string, title, and sub-Frame status
// of the web page (i.e. whether the web page is in a frame). It accepts
// the history entry object as the 1st parameter, and the session history
// index as the 2nd one. 
function  testHistoryEntry(entry, index)
{
  if (!entry)
    alert("Didn't get history entry object");

   // Get URI for the  next Entry
   Uri = entry.URI;
   uriSpec = Uri.spec;
   dump("The URI = " + uriSpec);

   // Get Title for the nextEntry
   title = entry.title;
   dump("The page title = " + title);
    
   // Get SubFrame Status for the nextEntry
   frameStatus = entry.isSubFrame;
   dump("The subframe value = " + frameStatus);
}

// *************************************************************************
// testHistoryEntryUri() returns the URI string for a given index. It accepts
// the history entry object as the 1st parameter, and the session history
// index as the 2nd one.
function  testHistoryEntryUri(nextHE, index)
{
  if (!nextHE) {
    alert("Didn't get history entry object");
    return null;
  }
   // Get URI for the  next Entry
   Uri = entry.URI;
   uriSpec = Uri.spec;
   
   return uriSpec;
}

// *************************************************************************
// testHistoryEntryTitle() returns the web page title for a given index. It accepts
// the history entry object as the 1st parameter, and the session history
// index as the 2nd one.
function  testHistoryEntryTitle(nextHE, index)
{
  if (!nextHE) {
    alert("Didn't get history entry object");
    return null;
  }
   // Get Title for the nextEntry
   title = entry.title;
   
   return title;
}

// *************************************************************************
// testHistoryEntryFrame() returns the sub-frame status for a given index. It accepts
// the history entry object as the 1st parameter, and the session history
// index as the 2nd one.
function  testHistoryEntryFrame(nextHE, index)
{
  if (!nextHE) {
    alert("Didn't get history entry object");
    return null;
  }
  // Get SubFrame Status for the nextEntry
   frameStatus = entry.isSubFrame;
   
   return frameStatus;
}
  
 // *************************************************************************
// purgeEntries() purges entries in the session history. It accepts the session
// history object as the 1st parameter. The 2nd parameter is for the number of
// entries to purge (it will purge the earliest entries of the session history).
function purgeEntries(sHistory, numEntries)
{
  if (!sHistory) {
    alert("Didn't get SessionHistory object"); 
  }         
  sHistory.PurgeHistory(numEntries);        
} 

// *************************************************************************
// setSHistoryListener() adds a session history listener. This enables session
// history events to be tracked (i.e. url load, forward, back, ...). A listener
// and the session history object are the input parameters.
function setSHistoryListener(sHistory, listener)
{
  dump("Inside nsISHistoryTestLib.js: setSHistoryListener()");
  if (!sHistory) {
    alert("Didn't get SessionHistory object");
  }
    netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserAccess");
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");       
    sHistory.addSHistoryListener(listener);
}