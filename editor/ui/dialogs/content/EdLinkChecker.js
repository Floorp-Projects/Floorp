/* 
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Akkana Peck (akkana@netscape.com)
 *   Charles Manxke (cmanske@netscape.com)
 */

// Variables used across all the links being checked:
var gNumLinksToCheck = 0;     // The number of nsILinkCheckers
var gLinksBeingChecked = {};  // Array of nsIURICheckers
var gURIRefObjects = {};      // Array of nsIURIRefObjects
var gNumLinksCalledBack = 0;
var gStartedAllChecks = false;
var gLinkCheckTimerID = 0;

// Implement nsIRequestObserver:
var gRequestObserver =
{
  // urichecker requires that we have an OnStartRequest even tho it's a nop.
  onStartRequest: function(request, ctxt) { },

  // onStopRequest is where we really handle the status.
  onStopRequest: function(request, ctxt, status)
  {
    var linkChecker = request.QueryInterface(Components.interfaces.nsIURIChecker);
    if (linkChecker)
    {
      gNumLinksCalledBack++;
      linkChecker.status = status;
      for (var i = 0; i < gNumLinksCalledBack; i++)
      {
        if (linkChecker ==  gLinksBeingChecked[i])
          gLinksBeingChecked[i].status = status;
      }

      if (gStartedAllChecks && gNumLinksCalledBack >= gNumLinksToCheck)
      {
        clearTimeout(gLinkCheckTimerID);
        LinkCheckTimeOut();
      }
    }
  }
}

function Startup()
{
  if (!InitEditorShell())
    return;

  // Set window location relative to parent window (based on persisted attributes)
  SetWindowLocation();
  // Get all objects that refer to other locations
  var objects = editorShell.GetLinkedObjects();

  // Loop over the nodes that have links:
  for (var i = 0; i < objects.Count(); i++)
  {
    var refobj = objects.GetElementAt(gNumLinksToCheck).QueryInterface(Components.interfaces.nsIURIRefObject);
    // Loop over the links in this node:
    if (refobj)
    {
      try {
        var uri;
        while ((uri = refobj.GetNextURI()))
        {
          // Use the real class in netlib:
          // Note that there may be more than one link per refobj
          gURIRefObjects[gNumLinksToCheck] = refobj;

          // Make a new nsIURIChecker
          gLinksBeingChecked[gNumLinksToCheck]
            = Components.classes["@mozilla.org/network/urichecker;1"]
                .createInstance()
                  .QueryInterface(Components.interfaces.nsIURIChecker);

          // XXX Calling this is related to crash deleting gLinksBeingChecked when dialog is closed
          //     (if this isn't called, no crash)
          gLinksBeingChecked[gNumLinksToCheck].asyncCheckURI(uri, gRequestObserver, null,
							     Components.interfaces.nsIRequest.LOAD_NORMAL);
          gNumLinksToCheck++;
        };
      } catch (e) {
dump(" Exception ERROR in Link checker. e.result="+e.result+", gNumLinksToCheck="+gNumLinksToCheck+"\n");
      }
    }
  }
  // Done with the loop, now we can be prepared for the finish:
  gStartedAllChecks = true;

  // Start timer to limit how long we wait for link checking
  gLinkCheckTimerID = setTimeout("LinkCheckTimeOut()", 5000);
}

function LinkCheckTimeOut()
{
  // We might have gotten here via a late timeout
  if (gNumLinksToCheck <= 0)
    return;
  gLinkCheckTimerID = 0;

dump("*** LinkCheckTimeout: Heard from " + gNumLinksCalledBack + " of "+gNumLinksToCheck + "\n");

  gNumLinksToCheck = 0;
  gStartedAllChecks = false;
  if ("length" in gLinksBeingChecked)
  {
    for (var i=0; i < gLinksBeingChecked.length; i++)
    {
      var linkChecker = gLinksBeingChecked[i].QueryInterface(Components.interfaces.nsIURIChecker);
      // nsIURIChecker status values:
      // NS_BINDING_SUCCEEDED     link is valid
      // NS_BINDING_FAILED        link is invalid (gave an error)
      // NS_BINDING_ABORTED       timed out, or cancelled
      switch (linkChecker.status)
      {
        case 0:           // NS_BINDING_SUCCEEDED
          dump("   " + linkChecker.name + " OK!\n");
          break;
        case 0x804b0001:  // NS_BINDING_FAILED
          dump(">> " + linkChecker.name + " is broken\n");
          break;
        case 0x804b0002:   // NS_BINDING_ABORTED
          dump(">> " + linkChecker.name + " timed out\n");
          break;
        default:
          dump(">> " + linkChecker.name + " not checked\n");
          break;
      }
    }
  }
}

function ChangeUrl()
{
}

function onAccept()
{
  SaveWindowLocation();
  return true; // do close the window
}

function onCancelLinkChecker()
{
  if (gLinkCheckTimerID)
    clearTimeout(gLinkCheckTimerID);

/*
  LinkCheckTimeOut();
*/
  return onCancel();
}
