/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 */

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

var nsIMsgSearchValidityManager = Components.interfaces.nsIMsgSearchValidityManager;

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

    gFilterNameElement.focus();
    doSetOKCancel(onOk, null);
    moveToAlertPosition();
}

function onOk()
{
    if (isDuplicateFilterNameExists())
    {
        var commonDialogsService
            = Components.classes["@mozilla.org/appshell/commonDialogs;1"].getService();
        commonDialogsService
            = commonDialogsService.QueryInterface(Components.interfaces.nsICommonDialogs);

        if (commonDialogsService)
        {
            commonDialogsService.Alert(window,
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

function isDuplicateFilterNameExists()
{
    var args = window.arguments[0];
    var myFilterList;
    var filterName= gFilterNameElement.value;

    if ("filterList" in args)
        myFilterList = args.filterList;

    if (myFilterList) {
        for (var i = 0; i < myFilterList.filterCount; i++)
        {
            if (filterName == myFilterList.getFilterAt(i).filterName)
                return true;
        }
    }

    return false;
}

function getScopeFromFilterList(filterList)
{
    if (!filterList) return false;
    var type = filterList.folder.server.type;
    if (type == "nntp") return nsIMsgSearchValidityManager.news;
    if (type == "pop3") return nsIMsgSearchValidityManager.offlineMail;
    return nsIMsgSearchValidityManager.onlineMailFilter;
}

function getScope(filter) {
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
        // there are multiple sub-items that have given attribute
        var targets = gActionTargetElement.getElementsByAttribute("value", filter.actionTargetFolderUri);

        if (targets && targets.length > 0) {
            var target = targets[0];
            if (target.localName == "menuitem"){
                gActionTargetElement.selectedItem = target;
                PickedMsgFolder(gActionTargetElement.selectedItem, gActionTargetElement.id)
            }
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
            targetUri = gActionTargetElement.selectedItem.getAttribute("value");
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
    } else {
        gFilter.filterName = gFilterNameElement.value;
        isNewFilter = false;
    }

    gFilter.enabled=true;
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
    gActionValueDeck.setAttribute("index", menuitem.getAttribute("actionvalueindex"));
}

function GetFirstSelectedMsgFolder()
{
    var selectedFolder = gActionTargetElement.selectedItem.getAttribute("value");

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
