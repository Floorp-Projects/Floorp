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
var gFilterNameElement;
var gActionElement;
var gActionTargetElement;


var nsIMsgSearchValidityManager = Components.interfaces.nsIMsgSearchValidityManager;

var nsMsgFilterAction = Components.interfaces.nsMsgFilterAction;

function filterEditorOnLoad()
{
    initializeSearchWidgets();
    initializeFilterWidgets();
    if (window.arguments && window.arguments[0]) {
        var args = window.arguments[0];
        if (args.filter) {
            gFilter = window.arguments[0].filter;

            initializeDialog(gFilter);
        } else {
            gFilterList = args.filterList;
            if (gFilterList)
                setSearchScope(getScopeFromFilterList(gFilterList));
        }
    }

    doSetOKCancel(onOk, null);
}

function onOk()
{
    saveFilter();

    // parent should refresh filter list..
    // this should REALLY only happen when some criteria changes that
    // are displayed in the filter dialog, like the filter name
    window.arguments[0].refresh = true;
    window.close();
}

function getScopeFromFilterList(filterList)
{
    if (!filterList) return;
    var type = filterList.folder.server.type;
    if (type == "nntp") return nsIMsgSearchValidityManager.news;
    return nsIMsgSearchValidityManager.onlineMail;
}

function getScope(filter) {
    return getScopeFromFilterList(filter.filterList);
}

function initializeFilterWidgets()
{
    gFilterNameElement = document.getElementById("filterName");
    gActionElement = document.getElementById("actionMenu");
    gActionTargetElement = document.getElementById("actionTargetFolder");
}

function initializeDialog(filter)
{
    gFilterNameElement.value = filter.filterName;

    gActionElement.selectedItem=gActionElement.getElementsByAttribute("data", filter.action)[0];


    if (filter.action == nsMsgFilterAction.MoveToFolder) {
        // there are multiple sub-items that have given attribute
        var targets = gActionTargetElement.getElementsByAttribute("data", filter.actionTargetFolderUri);

        if (targets && targets.length) {
            var target = targets[0];
            if (target.tagName == "menuitem")
                gActionTargetElement.selectedItem = target;
        }
    }
        

    var scope = getScope(filter);
    setSearchScope(scope);
    initializeSearchRows(scope, filter.searchTerms)
}


// move to overlay

function saveFilter() {

    var isNewFilter;
    if (!gFilter) {
        gFilter = gFilterList.createFilter(gFilterNameElement.value);
        isNewFilter = true;
    } else {
        gFilter.filterName = gFilterNameElement.value;
        isNewFilter = false;
    }

    saveSearchTerms(gFilter.searchTerms, gFilter);

    var action = gActionElement.selectedItem.getAttribute("data");
    gFilter.action = action;
    if (action == nsMsgFilterAction.MoveToFolder &&
        gActionElement.selectedItem)
        gFilter.actionTargetFolderUri =
            gActionTargetElement.selectedItem.getAttribute("data");
    else if (action == nsMsgFilterAction.ChangePriority)
        gFilter.actionPriority = 0; // whatever, fix this

    if (isNewFilter)
        gFilterList.insertFilterAt(0, gFilter);
}

