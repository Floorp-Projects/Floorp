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
var gSearchScope;

// cache the key elements we need
var gFilterNameElement;
var gActionElement;
var gActionTargetElement;


// search stuff (move to overlay)
var gTotalSearchTerms=0;
var gSearchRowContainer;
var gSearchTermContainer;
var gSearchRemovedTerms = new Array;

var nsIMsgSearchValidityManager = Components.interfaces.nsIMsgSearchValidityManager;

var nsMsgFilterAction = Components.interfaces.nsMsgFilterAction;
var nsIMsgSearchTerm = Components.interfaces.nsIMsgSearchTerm;

function filterEditorOnLoad()
{
    initializeSearchWidgets();
    if (window.arguments && window.arguments[0]) {
        var args = window.arguments[0];
        if (args.filter) {
            gFilter = window.arguments[0].filter;

            initializeDialog(gFilter);
        } else {
            if (args.filterList)
                setSearchScope(getScopeFromFilterList(args.filterList));
        }
    }

    doSetOKCancel(onOk, null);
}

function onOk()
{
    saveFilter();
    window.close();
}

// move to overlay
// set scope on all visible searhattribute tags
function setSearchScope(scope) {
    gSearchScope = scope;
    var searchTermElements = gSearchTermContainer.childNodes;
    if (!searchTermElements) return;
    for (var i=0; i<searchTermElements.length; i++) {
        searchTermElements[i].searchattribute.searchScope = scope;
    }
}

function booleanChanged(event) {
    dump("Boolean is now " + event.target.data + "\n");
}

function getScopeFromFilterList(filterList)
{
    var type = filterList.folder.server.type;
    if (type == "nntp") return nsIMsgSearchValidityManager.news;
    return nsIMsgSearchValidityManager.onlineMail;
}

function getScope(filter) {
    return getScopeFromFilterList(filter.filterList);
}

function initializeDialog(filter)
{
    gFilterNameElement = document.getElementById("filterName");
    gFilterNameElement.value = filter.filterName;

    gActionElement = document.getElementById("actionMenu");
    gActionElement.selectedItem=gActionElement.getElementsByAttribute("data", filter.action)[0];

    gActionTargetElement = document.getElementById("actionTargetFolder");
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

function initializeSearchWidgets() {
    gSearchRowContainer = document.getElementById("searchTermList");
    gSearchTermContainer = document.getElementById("searchterms");
}

// move to overlay
function initializeSearchRows(scope, searchTerms)
{
    initializeSearchWidgets();
    gTotalSearchTerms = searchTerms.Count();
    for (var i=0; i<gTotalSearchTerms; i++) {
        var searchTerm = searchTerms.QueryElementAt(i, nsIMsgSearchTerm);
        createSearchRow(i, scope, searchTerm);
    }
}

// move to overlay
function createSearchRow(index, scope, searchTerm)
{
    var searchAttr = document.createElement("searchattribute");
    var searchOp = document.createElement("searchoperator");
    var searchVal = document.createElement("searchvalue");

    // now set up ids:
    searchAttr.id = "searchAttr" + index;
    searchOp.id  = "searchOp" + index;
    searchVal.id = "searchVal" + index;

    searchAttr.setAttribute("for", searchOp.id + "," + searchVal.id);

    var rowdata = new Array(null, searchAttr,
                            null, searchOp,
                            null, searchVal,
                            null);
    var searchrow = constructRow(rowdata);

    searchrow.id = "searchRow" + index;

    // should this be done with XBL or just straight JS?
    // probably straight JS but I don't know how that's done.
    var searchTermElement = document.createElement("searchterm");
    searchTermElement.id = "searchTerm" + index;
    gSearchTermContainer.appendChild(searchTermElement);
    searchTermElement = document.getElementById(searchTermElement.id);
    
    searchTermElement.searchattribute = searchAttr;
    searchTermElement.searchoperator = searchOp;
    searchTermElement.searchvalue = searchVal;

    // and/or string handling:
    // this is scary - basically we want to take every other
    // treecell, (note the i+=2) which will be a text label,
    // and set the searchTermElement's
    // booleanNodes to that
    var stringNodes = new Array;
    var treecells = searchrow.firstChild.childNodes;
    var j=0;
    for (var i=0; i<treecells.length; i+=2) {
        stringNodes[j++] = treecells[i];
    }
    searchTermElement.booleanNodes = stringNodes;
    
    gSearchRowContainer.appendChild(searchrow);

    searchTermElement.searchScope = scope;
    if (searchTerm)
        searchTermElement.searchTerm = searchTerm;

}

// creates a <treerow> using the array treeCellChildren as 
// the children of each treecell
function constructRow(treeCellChildren)
{
    var treeitem = document.createElement("treeitem");
    var row = document.createElement("treerow");
    for (var i = 0; i<treeCellChildren.length; i++) {
      var treecell = document.createElement("treecell");
      
      // it's ok to have empty cells
      if (treeCellChildren[i]) {
          treecell.setAttribute("allowevents", "true");
          treeCellChildren[i].setAttribute("flex", "1");
          treecell.appendChild(treeCellChildren[i]);
      }
      row.appendChild(treecell);
    }
    treeitem.appendChild(row);
    return treeitem;
}

function removeSearchRow(index)
{
    dump("removing search term " + index + "\n");
    var searchTermElement = document.getElementById("searchTerm" + index);
    if (!searchTermElement) {
        dump("removeSearchRow: couldn't find search term " + index + "\n");
        return;
    }

    // need to remove row from tree, so walk upwards from the
    // searchattribute to find the first <treeitem>
    var treeItemRow = searchTermElement.searchattribute;
    while (treeItemRow) {
        if (treeItemRow.tagName == "treeitem") break;
        treeItemRow = treeItemRow.parentNode;
    }

    if (!treeItemRow) {
        dump("Error: couldn't find parent treeitem!\n");
        return;
    }


    if (searchTermElement.searchTerm) {
        dump("That was a real row! queuing " + searchTermElement.searchTerm + " for disposal\n");
        gSearchRemovedTerms[gSearchRemovedTerms.length] = searchTermElement.searchTerm;
    } else {
        dump("That wasn't real. ignoring \n");
    }
    
    treeItemRow.parentNode.removeChild(treeItemRow);
    searchTermElement.parentNode.removeChild(searchTermElement);
}

function saveFilter() {

    if (!gFilter) {
        gFilter = gFilterList.createFilter(gFilterNameElement.value);
    } else {
        gFilter.filterName = gFilterNameElement.value;
    }

    var searchTermElements =
        document.getElementById("searchterms").childNodes;
    
    var searchTerms = gFilter.searchTerms;
    
    for (var i = 0; i<searchTermElements.length; i++) {
        try {
            dump("Saving search element " + i + "\n");
            var searchTerm = searchTermElements[i].searchTerm;
            if (searchTerm)
                searchTermElements[i].save();
            else {
                // need to create a new searchTerm, and somehow save it to that
                dump("Need to create searchterm " + i + "\n");
                searchTerm = gFilter.createTerm();
                searchTermElements[i].saveTo(searchTerm);
                gFilter.appendTerm(searchTerm);
            }
        } catch (ex) {

            dump("** Error: " + ex + "\n");
        }
    }

    var searchTerms = gFilter.searchTerms;
    // now remove the queued elements
    dump("Removing " + gSearchRemovedTerms.length + "\n");
    for (var i=0; i<gSearchRemovedTerms.length; i++) {
        // this is so nasty, we have to iterate through
        // because GetIndexOf is acting funny
        var searchTermSupports = gSearchRemovedTerms[i].QueryInterface(Components.interfaces.nsISupports);
        dump("removing " + gSearchRemovedTerms[i] + "\n");
        searchTerms.RemoveElement(searchTermSupports);
    }

    var action = gActionElement.selectedItem.getAttribute("data");
    gFilter.action = action;
    if (action == nsMsgFilterAction.MoveToFolder &&
        gActionElement.selectedItem)
        gFilter.actionTargetFolderUri =
            gActionTargetElement.selectedItem.getAttribute("data");
    else if (action == nsMsgFilterAction.ChangePriority)
        gFilter.actionPriority = 0; // whatever, fix this
}

function onMore(event)
{
    createSearchRow(gTotalSearchTerms++, gSearchScope, null);
}

function onLess(event)
{
    removeSearchRow(--gTotalSearchTerms);
}
