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

// search stuff (move to overlay)
var gSearchRowContainer;
var gSearchTermContainer;

var nsIMsgSearchValidityManager = Components.interfaces.nsIMsgSearchValidityManager;

var nsMsgFilterAction = Components.interfaces.nsMsgFilterAction;
var nsIMsgSearchTerm = Components.interfaces.nsIMsgSearchTerm;

function filterEditorOnLoad()
{
    if (window.arguments && window.arguments[0]) {
        var args = window.arguments[0];
        if (args.filter) {
            gFilter = window.arguments[0].filter;
        
            initializeDialog(gFilter);
        } else {
            if (args.filterList)
                setScope(getScopeFromFilterList(args.filterList));
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
function setScope(scope) {
    var searchTermElements = gSearchTermContainer.childNodes;
    for (var i=0; i<searchTerms.length; i++) {
        searchTermElements[i].searchattribute.searchScope = scope;
    }
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
    
    initializeSearchRows(scope, filter.searchTerms)
}

// move to overlay
function initializeSearchRows(scope, searchTerms)
{
    gSearchRowContainer = document.getElementById("searchTermList");
    gSearchTermContainer = document.getElementById("searchterms");
    var numTerms = searchTerms.Count();
    for (var i=0; i<numTerms; i++) {
        createSearchRow(i);

      // now that it's been added to the document, we can initialize it.
      var searchTermElement = document.getElementById("searchTerm" + i);

      if (searchTermElement) {
          searchTermElement.searchScope = scope;
          var searchTerm =
              searchTerms.QueryElementAt(i, nsIMsgSearchTerm);
          if (searchTerm) searchTermElement.searchTerm = searchTerm;
      }
      else {

          dump("Ack! Can't find searchTerm" + i + "!\n");
      }
    }

}

// move to overlay
function createSearchRow(index)
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
    var searchTerm = document.createElement("searchterm");
    searchTerm.id = "searchTerm" + index;
    gSearchTermContainer.appendChild(searchTerm);
    searchTerm = document.getElementById(searchTerm.id);
    
    searchTerm.searchattribute = searchAttr;
    searchTerm.searchoperator = searchOp;
    searchTerm.searchvalue = searchVal;

    // this is scary - basically we want to take every other
    // treecell, which will be a text label, and set the searchTerm's
    // booleanNodes to that
    var stringNodes = new Array;
    var treecells = searchrow.firstChild.childNodes;
    var j=0;
    for (var i=0; i<treecells.length; i+=2) {
        stringNodes[j++] = treecells[i];
    }
    searchTerm.booleanNodes = stringNodes;
    
    gSearchRowContainer.appendChild(searchrow);
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

function saveFilter() {

    gFilter.filterName = gFilterNameElement.value;

    var searchTermElements =
        document.getElementById("searchterms").childNodes;
    
    var searchTerms = gFilter.searchTerms;
    
    for (var i = 0; i<searchTermElements.length; i++) {
        try {
            searchTermElements[i].save();
        } catch (ex) {
            dump("**** error: " + ex + "\n");
        }
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
