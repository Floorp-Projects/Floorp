/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Platform for Privacy Preferences.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Samir Gehani <sgehani@netscape.com>
 *                 Harish Dhurvasula <harishd@netscape.com>
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

var gTopWin = null;
var gTopDoc = null;
var gIOService = null;

function initTopDocAndWin()
{
  if ("arguments" in window && window.arguments.length > 0 && 
      window.arguments[0])
  {
    gTopWin = null;
    gTopDoc = window.arguments[0];
  }
  else 
  {
    if ("gBrowser" in window.opener)
      gTopWin = window.opener.gBrowser.contentWindow;
    else
      gTopWin = window.opener.frames[0];
    gTopDoc = gTopWin.document;
  }
}

function initP3PTab()
{
  initTopDocAndWin();

  var mainLinkNode = document.getElementById("mainLink");
  mainLinkNode.setAttribute("label", gTopDoc.location.href);
  
  // now select the main link
  var tree = document.getElementById("linkList");
  tree.view.selection.select(0);

  var linkTypes = 
  [
  //  Tag       Attr         List node ID
    ["a",      "href",      "linkKids"],
    ["applet", "code",      "appletKids"],
    ["area",   "href",      "imageMapKids"],
    ["form",   "action",    "formKids"], 
    ["frame",  "src",       "frameKids"], 
    ["iframe", "src",       "frameKids"],
    ["img",    "src",       "imageKids"],
    ["image",  "src",       "imageKids"],
    ["link",   "href",      "externalDocKids"],
    ["object", "codebase",  "objectKids"],
	 ["object", "data",      "objectKids"],
    ["script", "src",       "scriptKids"]
  ];

  var list, i, j;
  var length = linkTypes.length;
  for (i = 0; i < length; ++i)
  {
    list = getLinksFor(linkTypes[i][0], linkTypes[i][1], gTopWin, gTopDoc);
    // now add list items under appropriate link type in the tree
    var len = list.length;
    for (j = 0; j < len; ++j)
    {
      addRow(linkTypes[i][2], list[j]);
    }
  }
}

function makeURLAbsolute(url, base)
{
  if (url.indexOf(':') > -1)
    return url;

  if (!gIOService) {
    gIOService = 
      Components.classes["@mozilla.org/network/io-service;1"].getService(nsIIOService);
  }

  var baseURI = gIOService.newURI(base, null, null);

  return gIOService.newURI(baseURI.resolve(url), null, null).spec;
}

function getLinksFor(aTagName, aAttrName, aWin, aDoc)
{
  var i, frame, length, list = new Array;

  // cycle through frames
  if (aWin)
  {
    length = aWin.frames.length;
    for (i = 0; i < length; ++i)
    {
      frame = aWin.frames[i];
      list = list.concat(getLinksFor(aTagName, aAttrName, 
                                     frame, frame.document));
    }
  }

  // now look for tags in the leaf document
  var elts = aDoc.getElementsByTagName(aTagName);
  length = elts.length;
  for (i = 0; i < length; ++i)
  {
    var url = elts[i][aAttrName];
    if (url) {
      try {
        // The user sees the links as absolute when mousing over.
        // The link list does not show the base URL for the user,
        // so this is the only way we can indicate where things
        // really point to in the link list.
        list.push(makeURLAbsolute(url, elts[i].baseURI));
      } catch (e) {
        // XXX Ignore for now, most likely bad URL, but could also be 
        //     something really serious like out of memory.
      }
    }
  }

  return list;
}

function addRow(aRootID, aLabel)
{
  const kXULNS = 
    "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
  var root = document.getElementById(aRootID);
  var item = document.createElementNS(kXULNS, "treeitem");
  root.appendChild(item);
  var row = document.createElementNS(kXULNS, "treerow");
  item.appendChild(row);
  var cell = document.createElementNS(kXULNS, "treecell");
  cell.setAttribute("label", aLabel);
  row.appendChild(cell);
}

function getSelectedURI()
{
  var URI = null;
  var tree = document.getElementById("linkList");
  var selectedItem = tree.contentView.getItemAtIndex(tree.currentIndex);
  if (selectedItem)
  {
    var selectedRow = 
      selectedItem.getElementsByTagName("treerow")[0];
    if (selectedRow)
    {
      var selectedCell = 
        selectedRow.getElementsByTagName("treecell")[0];
      if (selectedCell)
        URI = selectedCell.getAttribute("label");
    }
  }

  return URI;
}

//---------------------------------------------------------------------------
//   Interface to P3P backend
//---------------------------------------------------------------------------
var gPolicyViewer = null; // one policy viewer per page info window
var gPrivacyTabInfo = null;

function onMachineReadable()
{
  if (!gPolicyViewer) {
    gPolicyViewer = new nsPolicyViewer(gTopDoc);
  }
  gPolicyViewer.load(getSelectedURI(), LOAD_SUMMARY);
}

function onHumanReadable()
{
  if (!gPolicyViewer) {
    gPolicyViewer = new nsPolicyViewer(gTopDoc);
  }
  gPolicyViewer.load(getSelectedURI(), LOAD_POLICY);

}

function onOptInOptOut()
{
  if (!gPolicyViewer) {
    gPolicyViewer = new nsPolicyViewer(gTopDoc);
  }
  gPolicyViewer.load(getSelectedURI(), LOAD_OPTIONS);

}

function finalizePolicyViewer()
{
  if (gPolicyViewer)
    gPolicyViewer.finalize();
}

addEventListener("unload", finalizePolicyViewer, false);

