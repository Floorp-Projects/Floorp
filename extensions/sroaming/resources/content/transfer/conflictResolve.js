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
 *   ManyOne <http://www.manyone.net>
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

/* Code for the conflictResolve.xul dialog, which lets the user select
   which of 2 conflicting versions of the same file to keep. */

var params = window.arguments[0].QueryInterface(
                                   Components.interfaces.nsIDialogParamBlock);
var gIsDownload = true; // otherwise upload
var gCount = 0; // number of files

function Startup()
{
  centerWindowOnScreen();
  LoadElements();
  ClearParam();
  window.sizeToContent();
}

function AddItem(fileid, filename, server, local)
{
  /*
    <rows id="filerows">
      <row>
        <description><html:b>&filedescr.label;</html:b></description>
        <description><html:b>&server.label;</html:b></description>
        <description><html:b>&local.label;</html:b></description>
      </row>
      <radiogroup value="2"
                  style="display: -moz-grid-line;
                         -moz-box-orient: horizontal;"
                  flex="1">   <!-- acts as <column> -->
        <description>file description</description>
        <radio label="last modified server, size server" value="1"/>
        <radio label="last modified local, size local" value="2"/>
      </radiogroup>
    <rows>
  */

  var radiogroup = document.createElement("radiogroup");
  radiogroup.setAttribute("id", fileid);
  radiogroup.setAttribute("value", gIsDownload ? "2" : "1");
  var cell;

  cell = document.createElement("description");
  cell.appendChild(document.createTextNode(GetFileDescription(filename)));
  radiogroup.appendChild(cell);

  cell = document.createElement("radio");
  cell.setAttribute("value", "1");
  if (server)
    cell.setAttribute("label", server);
  radiogroup.appendChild(cell);

  cell = document.createElement("radio");
  cell.setAttribute("value", "2");
  if (local)
    cell.setAttribute("label", local);
  radiogroup.appendChild(cell);

  var rows = document.getElementById("filerows");
  rows.appendChild(radiogroup);

  // WORKAROUND needs to be here, due to bug 209594
  radiogroup.setAttribute("style",
                      "display: -moz-grid-line; -moz-box-orient: horizontal;");
}

function FileLabel(lastModified, size)
{
  if (!lastModified || lastModified == "" || !size || size == "")
    return GetString("NonExistant");
    //return GetString("FileStatsUnknown");

  var dateString = new Date(Number(lastModified)).toLocaleString();
  return GetString("FileStats")
         .replace(/%date%/, dateString)
         .replace(/%size%/, size);
}

function LoadElements()
{
  //For definition of meaning of params, see mozSRoaming.cpp::ConflictResolveUI

  // download
  var direction = params.GetInt(0);
  //ddump("Passing in: Int 0 (direction) is " + direction);
  if (direction != 1 && direction != 2)
    dumpError("Bad direction param");
  gIsDownload = direction == 1;

  // count
  gCount = params.GetInt(1);
  //ddump("Passing in: Int 1 (count) is " + gCount);
  if (gCount < 1)
    dumpError("Bad count param");

  // filenames
  for (var i = 0; i < gCount; i++)
  {
    var value = params.GetString(i + 1);
    //ddump("Passing in: String " + (i + 1) + " is " + value);
    var values = value.split(",");
    if (values.length != 1 && values.length != 5)
      dumpError("Bad file param");
    if (values.length == 5)
    {
      var server = FileLabel(values[1], values[2]);
      var local = FileLabel(values[3], values[4]);
      AddItem("file" + i, values[0], server, local);
    }
    else
      AddItem("file" + i, values[0]);
  }

  var descr_deck = document.getElementById("intro");
  descr_deck.setAttribute("selectedIndex", gIsDownload ? 0 : 1);
}

/* Because the param block has a different meaning on open and close, there
   is a risk of misinterpreting the input as output. Thus, clear it to avoid
   that. (0 values are intentionally not defined as legal.) */
function ClearParam()
{
  params.SetInt(0, 0);
  params.SetInt(1, 0);
}

function onOK()
{
  //ddump("onOK()");
  params.SetInt(0, 3); // OK
  for (var i = 0; i < gCount; i++)
  {
    var radiogroup = document.getElementById("file" + i);
    var choice = Number(radiogroup.value);
    if (choice != 1 && choice != 2)
    {
      dumpError("Bad radiogroup value: -" + radiogroup.value +
                "- -" + choice + "-");
      onCancel();
    }
    params.SetInt(i + 1, choice);
    //ddump("Passing back: Int " + (i + 1) + " is " + choice);
  }
  return true;
}

function onCancel()
{
  //ddump("onCancel()");
  params.SetInt(0, 4); // Cancel
  return true;
}
