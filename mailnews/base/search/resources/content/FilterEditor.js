/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Alec Flett <alecf@netscape.com>
 * Håkan Waara <hwaara@chello.se>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// the actual filter that we're editing
var gFilter;

// cache the key elements we need
var gFilterList;
var gFilterNameElement;
var gActionElement;
var gActionTargetElement;
var gActionValueDeck;
var gActionPriority;
var gFilterBundle;

var nsMsgSearchScope = Components.interfaces.nsMsgSearchScope;

var nsMsgFilterAction = Components.interfaces.nsMsgFilterAction;

function filterEditorOnLoad()
{
    initializeSearchWidgets();
    initializeFilterWidgets();

    gFilterBundle = document.getElementById("bundle_filter");
    if ("arguments" in window && window.arguments[0]) {
        var args = window.arguments[0];
        if ("filter" in args) {
            gFilter = window.arguments[0].filter;

            initializeDialog(gFilter);
        } else {
            gFilterList = args.filterList;
            if (gFilterList)
                setSearchScope(getScopeFromFilterList(gFilterList));
            // fake the first more button press
            onMore(null);
        }
    }
    if (!gFilter)
    {
      var stub = gFilterBundle.getString("untitledFilterName");
      var count = 1;
      var name = stub;

      // Set the default filter name to be "untitled filter"
      while (duplicateFilterNameExists(name)) 
      {
        count++;
        name = stub + " " + count.toString();
      }
      gFilterNameElement.value = name;
    }
    gFilterNameElement.focus();
    doSetOKCancel(onOk, null);
    moveToAlertPosition();
}

function onEnterInSearchTerm()
{
  // do nothing.  onOk() will get called since this is a dialog
}

function onOk()
{
    if (duplicateFilterNameExists(gFilterNameElement.value))
    {
        var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
        promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService);

        if (promptService)
        {
            promptService.alert(window,
                gFilterBundle.getString("cannotHaveDuplicateFilterTitle"),
                gFilterBundle.getString("cannotHaveDuplicateFilterMessage")
            );
        }

        return;
    }


    if (!saveFilter()) return;

    // parent should refresh filter list..
    // this should REALLY only happen when some criteria changes that
    // are displayed in the filter dialog, like the filter name
    window.arguments[0].refresh = true;
    window.close();
}

function duplicateFilterNameExists(filterName)
{
    var args = window.arguments[0];
    var filterList;
    if ("filterList" in args)
      filterList = args.filterList;
    if (filterList)
      for (var i = 0; i < filterList.filterCount; i++)
     {
       if (filterName == filterList.getFilterAt(i).filterName)
         return true;
     }
    return false;   
}

function getScopeFromFilterList(filterList)
{
    if (!filterList) {
      dump("yikes, null filterList\n");
      return nsMsgSearchScope.offlineMail;
}

    return filterList.folder.server.filterScope;
}

function getScope(filter) 
{
    return getScopeFromFilterList(filter.filterList);
}

function initializeFilterWidgets()
{
    gFilterNameElement = document.getElementById("filterName");
    gActionElement = document.getElementById("actionMenu");
    gActionTargetElement = document.getElementById("actionTargetFolder");
    gActionValueDeck = document.getElementById("actionValueDeck");
    gActionPriority = document.getElementById("actionValuePriority");
}

function initializeDialog(filter)
{
    var selectedPriority;
    gFilterNameElement.value = filter.filterName;

    gActionElement.selectedItem=gActionElement.getElementsByAttribute("value", filter.action)[0];
    showActionElementFor(gActionElement.selectedItem);

    if (filter.action == nsMsgFilterAction.MoveToFolder) {
        // preselect target folder
        var target = filter.actionTargetFolderUri;

        if (target) {
            SetFolderPicker(target, gActionTargetElement.id);
        }
    } else if (filter.action == nsMsgFilterAction.ChangePriority) {
        // initialize priority
        selectedPriority = gActionPriority.getElementsByAttribute("value", filter.actionPriority);

        if (selectedPriority && selectedPriority.length > 0) {
            selectedPriority = selectedPriority[0];
            gActionPriority.selectedItem = selectedPriority;
        }
    }


    var scope = getScope(filter);
    setSearchScope(scope);
    initializeSearchRows(scope, filter.searchTerms);
    if (filter.searchTerms.Count() > 1)
        gSearchLessButton.removeAttribute("disabled", "false");

}


// move to overlay

function saveFilter() {
    var isNewFilter;
    var str;

    var filterName= gFilterNameElement.value;
    if (!filterName || filterName == "") {
        str = gFilterBundle.getString("mustEnterName");
        window.alert(str);
        gFilterNameElement.focus();
        return false;
    }

    var targetUri;
    var action = gActionElement.selectedItem.getAttribute("value");

    if (action == nsMsgFilterAction.MoveToFolder) {
        if (gActionTargetElement)
            targetUri = gActionTargetElement.getAttribute("uri");
        if (!targetUri || targetUri == "") {
            str = gFilterBundle.getString("mustSelectFolder");
            window.alert(str);
            return false;
        }
    }

    else if (action == nsMsgFilterAction.ChangePriority) {
        if (!gActionPriority.selectedItem) {
            str = gFilterBundle.getString("mustSelectPriority");
            window.alert(str);
            return false;
        }
    }

    // this must happen after everything has
    if (!gFilter) {
        gFilter = gFilterList.createFilter(gFilterNameElement.value);
        isNewFilter = true;
        gFilter.enabled=true;
    } else {
        gFilter.filterName = gFilterNameElement.value;
        isNewFilter = false;
    }

    gFilter.action = action;
    if (action == nsMsgFilterAction.MoveToFolder)
        gFilter.actionTargetFolderUri = targetUri;
    else if (action == nsMsgFilterAction.ChangePriority)
        gFilter.actionPriority = gActionPriority.selectedItem.getAttribute("value");

    saveSearchTerms(gFilter.searchTerms, gFilter);

    if (isNewFilter) {
        // new filter - insert into gFilterList
        gFilterList.insertFilterAt(0, gFilter);
    }

    // success!
    return true;
}

function onTargetFolderSelected(event)
{
    SetFolderPicker(event.target.id, gActionTargetElement.id);
}


function onActionChanged(event)
{
    var menuitem = event.target;
    showActionElementFor(menuitem);
}

function showActionElementFor(menuitem)
{
    if (!menuitem) return;
    var indexValue = menuitem.getAttribute("actionvalueindex");

    gActionValueDeck.setAttribute("index", indexValue);

    // Disable the "New Folder..." button if any other action than MoveToFolder is chosen
    document.getElementById("newFolderButton").setAttribute("disabled", indexValue == "0" ? "false" : "true");
}

function GetFirstSelectedMsgFolder()
{
    var selectedFolder = gActionTargetElement.getAttribute("uri");

    var msgFolder = GetMsgFolderFromUri(selectedFolder);
    return msgFolder;
}

function SearchNewFolderOkCallback(name,uri)
{
    var msgFolder = GetMsgFolderFromUri(uri);
    msgFolder.createSubfolder(name, null);

    var curFolder = uri+"/"+name;
    SetFolderPicker(curFolder, gActionTargetElement.id);
}

function UpdateAfterCustomHeaderChange()
{
  updateSearchAttributes();
}
