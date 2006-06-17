# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

// This is the RDF Resource we're dealing with.
var gResource;
// This is the set of fields that are visible in the window.
var gFields;
// ...and this is a parallel array that contains the RDF properties
// that they are associated with.
var gProperties;

function showDescription()
{
  initServices();
  initBMService();

  gResource = RDF.GetResource(window.arguments[0]);
 
  if (gResource == BMSVC.getBookmarksToolbarFolder()) {
    var description = BookmarksUtils.getLocaleString("description_PersonalToolbarFolder");
    var box = document.getElementById("description-box");
    box.hidden = false;
    var textNode = document.createTextNode(description);
    document.getElementById("bookmarkDescription").appendChild(textNode);
  }
}

function Init()
{

  // assume the user will press cancel (only used when creating new resources)
  window.arguments[1].ok = false;

  // This is the set of fields that are visible in the window.
  gFields     = ["name", "url", "shortcut", "description", "webpanel", "feedurl",
                 "microsummaryMenuList"];

  // ...and this is a parallel array that contains the RDF properties
  // that they are associated with.
  gProperties = [RDF.GetResource(gNC_NS+"Name"),
                 RDF.GetResource(gNC_NS+"URL"),
                 RDF.GetResource(gNC_NS+"ShortcutURL"),
                 RDF.GetResource(gNC_NS+"Description"),
                 RDF.GetResource(gNC_NS+"WebPanel"),
                 RDF.GetResource(gNC_NS+"FeedURL"),
                 RDF.GetResource(gNC_NS+"GeneratedTitle")];

  var x;
  // Initialize the properties panel by copying the values from the
  // RDF graph into the fields on screen.

  for (var i=0; i<gFields.length; ++i) {
    var field = document.getElementById(gFields[i]);
    var value = BMDS.GetTarget(gResource, gProperties[i], true);
    
    if (value)
      value = value.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;

    if (gFields[i] == "webpanel")
      field.checked = (value != undefined);
    else if (gFields[i] == "microsummaryMenuList") {
      if (MicrosummaryPicker.enabled)
        MicrosummaryPicker.init();
      else {
        var microsummaryRow = document.getElementById("microsummaryRow");
        microsummaryRow.setAttribute("hidden", "true");
      }
      continue;
    }
    else if (value) //make sure were aren't stuffing null into any fields
      field.value = value;
  }

  var nameNode = document.getElementById("name");
  document.title = document.title.replace(/\*\*bm_title\*\*/gi, nameNode.value);

  // if its a container, disable some things
  var isContainerFlag = RDFCU.IsContainer(BMDS, gResource);
  if (!isContainerFlag) {
    // XXX To do: the "RDFCU.IsContainer" call above only works for RDF sequences;
    //            if its not a RDF sequence, we should to more checking to see if
    //            the item in question is really a container of not.  A good example
    //            of this is the "File System" container.
  }

  var isLivemark = BookmarksUtils.resolveType(gResource) == "Livemark";
  var isSeparator = BookmarksUtils.resolveType(gResource) == "BookmarkSeparator";

  if (isContainerFlag || isSeparator) {
    // Hide the "Load in sidebar" checkbox unless it's a bookmark.
    var webpanelCheckbox = document.getElementById("webpanel");
    webpanelCheckbox.hidden = true;

    // If it is a folder, it has no URL or Keyword
    document.getElementById("locationrow").setAttribute("hidden", "true");
    document.getElementById("shortcutrow").setAttribute("hidden", "true");
    if (isSeparator) {
      document.getElementById("descriptionrow").setAttribute("hidden", "true");
    }
  }

  if (isLivemark) {
    document.getElementById("locationrow").hidden = true;
    document.getElementById("shortcutrow").hidden = true;
  } else {
    document.getElementById("feedurlrow").hidden = true;
  }

  sizeToContent();
  
  // set initial focus
  nameNode.focus();
  nameNode.select();
}


function Commit()
{
  var changed = false;

  // Grovel through the fields to see if any of the values have
  // changed. If so, update the RDF graph and force them to be saved
  // to disk.
  for (var i=0; i<gFields.length; ++i) {
    var field = document.getElementById(gFields[i]);

    if (! field)
      continue;

    if (gFields[i] == "microsummaryMenuList") {
      if (MicrosummaryPicker.enabled) {
        changed |= MicrosummaryPicker.commit();
        MicrosummaryPicker.destroy();
      }
      continue;
    }

    // Get the new value as a literal, using 'null' if the value is empty.
    var newValue = field.value;
    if (gFields[i] == "webpanel")
      newValue = field.checked ? "true" : undefined;
 
    var oldValue = BMDS.GetTarget(gResource, gProperties[i], true);

    if (oldValue)
      oldValue = oldValue.QueryInterface(Components.interfaces.nsIRDFLiteral);

    if (newValue && gFields[i] == "shortcut") {
      // shortcuts are always lowercased internally
      newValue = newValue.toLowerCase();
      // strip trailing and leading whitespace
      newValue = newValue.replace(/(^\s+|\s+$)/g, '');
    }
    else if (newValue && gFields[i] == "url") {
      if (newValue.indexOf(":") < 0)
        // we're dealing with the URL attribute;
        // if a scheme isn't specified, use "http://"
        newValue = "http://" + newValue;
    }

    if (newValue)
      newValue = RDF.GetLiteral(newValue);

    changed |= updateAttribute(gProperties[i], oldValue, newValue);

    if (gFields[i] == "url" && oldValue && oldValue.Value != newValue.Value) {
      // if the URL was updated, clear out the favicon
      var icon = BMDS.GetTarget(gResource, RDF.GetResource(gNC_NS+"Icon"), true);
      if (icon) BMDS.Unassert(gResource, RDF.GetResource(gNC_NS+"Icon"), icon);
    }
  }

  if (changed) {
    var remote = BMDS.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
    if (remote)
      remote.Flush();
  }

  window.arguments[1].ok = true;
  window.close();
  return true;
}

function Cancel()
{
  if (MicrosummaryPicker.enabled)
    MicrosummaryPicker.destroy();
  return true;
}

function updateAttribute(aProperty, aOldValue, aNewValue)
{
  if ((aOldValue || aNewValue) && aOldValue != aNewValue) {
    if (aOldValue && !aNewValue)
      BMDS.Unassert(gResource, aProperty, aOldValue);
    else if (!aOldValue && aNewValue)
      BMDS.Assert(gResource, aProperty, aNewValue, true);
    else /* if (aOldValue && aNewValue) */
      BMDS.Change(gResource, aProperty, aOldValue, aNewValue);
    return true;
  }
  return false;
}
