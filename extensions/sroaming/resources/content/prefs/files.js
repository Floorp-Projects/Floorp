/*-*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
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
 * The Original Code is Mozilla Roaming code.
 *
 * The Initial Developer of the Original Code is 
 *   Ben Bucksch <http://www.bucksch.org> of
 *   Beonex <http://www.beonex.com>
 * Portions created by the Initial Developer are Copyright (C) 2002-2004
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

/* See all.js */

var _elementIDs = []; // no prefs (nsIPrefBranch) needed, see above
var addedHandler = false; // see top.js

function Load()
{
  parent.initPanel('chrome://sroaming/content/prefs/files.xul');

  FileLabels();
  SetFiles();
  // dataManager.pageData doesn't work, because it needs to work on both panes
  if (!parent.roaming)
    parent.roaming = new RoamingPrefs();
  DataToUI();

  if (parent != null && "firefox" in parent)
  {
    if (!parent.firefox.filesLoad)
      parent.firefox.filesLoad = Load;
    if (!parent.firefox.filesUnload)
      parent.firefox.filesUnload = Unload;
  }

  if (!addedHandler)
  {
    addedHandler = true;
    parent.hPrefWindow.registerOKCallbackFunc(function()
    {
      UIToData();
      parent.roaming.okClicked();
    });
  }
}


// Logic

// some files have a non-static name. set these filenames here.
function SetFiles()
{
  var prefs = Components.classes["@mozilla.org/preferences;1"]
                .getService(Components.interfaces.nsIPrefBranch);
  try
  {
    SetFile("filePassword", prefs.getCharPref("signon.SignonFileName"));
    SetFile("fileWallet", prefs.getCharPref("wallet.SchemaValueFileName"));
  } catch (e) {}

  // disable the nodes which are still invalid
  var children = E("filesList").childNodes;
  for (var i = 0; i < children.length; i++)
  {
    var checkbox = children[i];
    if (!("getAttribute" in checkbox) ||
        checkbox.getAttribute("type") != "checkbox")
      continue;
    if (checkbox.getAttribute("filename") == "")
      checkbox.disabled = true;
  }
}

function SetFile(elementID, filename)
{
  var listitem = document.getElementById(elementID);
  listitem.setAttribute("filename", filename);
}


// UI

// write data to widgets
function DataToUI()
{
  var data = parent.roaming;
  var filesList = E("filesList");

  EnableTree(data.Enabled, filesList);

  // first, disable all by default
  var children = filesList.childNodes;
  for (var i = 0, l = children.length; i < l; i++)
  {
    var checkbox = children[i];
    if (!("getAttribute" in checkbox) ||
        checkbox.getAttribute("type") != "checkbox")
      // Somebody adds unwanted nodes as children to listbox :-(
      continue;
    checkbox.checked = false;
  }
  // then check for each file in the list, if it's in the checkboxes.
  // enabled it, if so, otherwise create and enable it (for files added by
  // the user).
  for (i = 0, l = data.Files.length; i < l; i++)
  {
    var file = data.Files[i];
    var found = false;
    for (var i2 = 0, l2 = children.length; i2 < l2; i2++)
    {
      var checkbox = children[i2];
      if ("getAttribute" in checkbox
          && checkbox.getAttribute("type") == "checkbox"
          // Somebody adds unwanted nodes as children to listbox :-(
          && checkbox.getAttribute("filename") == file)
      {
        checkbox.checked = true;
        found = true;
      }
    }
    if (!found)
    {
      var li = document.createElement("listitem");
      if (li)
      {
        li.setAttribute("type", "checkbox");
        li.setAttribute("id", file);
        li.setAttribute("filename", file);
        li.setAttribute("label", file);
        li.setAttribute("checked", "true");
        filesList.appendChild(li);
      }
    }
  }
}

// write widget content to data
function UIToData()
{
  var data = parent.roaming;
  data.Files = new Array(); // clear list
  var children = E("filesList").childNodes;
  for (var i = 0; i < children.length; i++)
  {
    var checkbox = children.item(i);
    if (checkbox.checked)
      data.Files.push(checkbox.getAttribute("filename"));
  }
  data.changed = true; // excessive
}

/* read human-readable names for profile files from filedescr.properties
   and use them as labels */
function FileLabels()
{
  var children = E("filesList").childNodes;
  for (var i = 0; i < children.length; i++)
  {
    var checkbox = children[i];
    if (!("getAttribute" in checkbox) ||
        checkbox.getAttribute("type") != "checkbox")
      continue;

    checkbox.setAttribute("label",
                          GetFileDescription(checkbox.getAttribute("filename"),
                                             checkbox.getAttribute("id")));
  }
}
