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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Akkana Peck (akkana@netscape.com)
 *   Charles Manxke (cmanske@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

// Variables used across all the links being checked:
var gNumLinksToCheck = 0;     // The number of nsILinkCheckers
var gLinksBeingChecked = [];  // Array of nsIURICheckers
var gURIRefObjects = [];      // Array of nsIURIRefObjects
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
        if (linkChecker == gLinksBeingChecked[i])
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
  var editor = GetCurrentEditor();
  if (!editor)
  {
    window.close();
    return;
  }

  // Get all objects that refer to other locations
  var objects;
  try {
    objects = editor.getLinkedObjects();
  } catch (e) {}

  if (!objects || objects.Count() == 0)
  {
    AlertWithTitle(GetString("Alert"), GetString("NoLinksToCheck"));
    window.close();
    return;
  }

  gDialog.LinksList = document.getElementById("LinksList");
  gDialog.Close     = document.documentElement.getButton("cancel");

  // Set window location relative to parent window (based on persisted attributes)
  SetWindowLocation();


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
          // XXX uri creation needs to be localized
          gLinksBeingChecked[gNumLinksToCheck].init(GetIOService().newURI(uri, null, null));
          gLinksBeingChecked[gNumLinksToCheck].asyncCheck(gRequestObserver, null);

          // Add item  
          var linkChecker = gLinksBeingChecked[gNumLinksToCheck].QueryInterface(Components.interfaces.nsIURIChecker);
          SetItemStatus(linkChecker.name, "busy");
dump(" *** Linkcount = "+gNumLinksToCheck+"\n");
          gNumLinksToCheck++;

        };
      } catch (e) { dump (" *** EXCEPTION\n");}
    }
  }
  // Done with the loop, now we can be prepared for the finish:
  gStartedAllChecks = true;

  // Start timer to limit how long we wait for link checking
  gLinkCheckTimerID = setTimeout("LinkCheckTimeOut()", 5000);
  window.sizeToContent();
}

function LinkCheckTimeOut()
{
  // We might have gotten here via a late timeout
  if (gNumLinksToCheck <= 0)
    return;
  gLinkCheckTimerID = 0;

  gNumLinksToCheck = 0;
  gStartedAllChecks = false;
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
        SetItemStatus(linkChecker.name, "done");
        break;
      case 0x804b0001:  // NS_BINDING_FAILED
        dump(">> " + linkChecker.name + " is broken\n");
      case 0x804b0002:   // NS_BINDING_ABORTED
//        dump(">> " + linkChecker.name + " timed out\n");
      default:
//        dump(">> " + linkChecker.name + " not checked\n");
        SetItemStatus(linkChecker.name, "failed");
        break;
    }
  }
  gDialog.Close.setAttribute("label", GetString("Close"));
}

// Add url to list of links to check
// or set status for file already in the list
// Returns true if url was in the list
function SetItemStatus(url, status)
{
  if (!url)
    return false;

  if (!status)
    status = "busy";

  // Just set attribute for status icon 
  // if we already have this url 
  var listitems = document.getElementsByTagName("listitem");
  if (listitems)
  {
    for (var i=0; i < listitems.length; i++)
    {
      if (listitems[i].getAttribute("label") == url)
      {
        listitems[i].setAttribute("progress", status);
        return true;
      }
    }
  }

  // We're adding a new item to list
  var listitem = document.createElementNS(XUL_NS, "listitem");
  if (listitem)
  {
    listitem.setAttribute("class", "listitem-iconic progressitem");
    // This triggers CSS to show icon for each status state
    listitem.setAttribute("progress", status);
    listitem.setAttribute("label", url);
    gDialog.LinksList.appendChild(listitem);
  }
  return false;
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
