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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 */

var gTotalSearchTerms=0;
var gSearchRowContainer;
var gSearchTermContainer;
var gSearchRemovedTerms = new Array;
var gSearchScope;
var gSearchLessButton;

var nsIMsgSearchTerm = Components.interfaces.nsIMsgSearchTerm;

function initializeSearchWidgets() {
    gSearchBooleanRadiogroup = document.getElementById("booleanAndGroup");
    gSearchRowContainer = document.getElementById("searchTermList");
    gSearchTermContainer = document.getElementById("searchterms");
    gSearchLessButton = document.getElementById("less");
    if (!gSearchLessButton)
        dump("I couldn't find less button!");
}

function initializeBooleanWidgets() {

    var booleanAnd = true;
    // get the boolean value from the first term
    var firstTerm = gSearchTermContainer.firstChild;
    if (firstTerm)
        booleanAnd = firstTerm.booleanAnd;

    // target radio items have data="and" or data="or"
    targetValue = "or";
    if (booleanAnd) targetValue = "and";
    
    targetElement = gSearchBooleanRadiogroup.getElementsByAttribute("data", targetValue)[0];
    
    gSearchBooleanRadiogroup.selectedItem = targetElement;
}

function initializeSearchRows(scope, searchTerms)
{
    gTotalSearchTerms = searchTerms.Count();
    for (var i=0; i<gTotalSearchTerms; i++) {
        var searchTerm = searchTerms.QueryElementAt(i, nsIMsgSearchTerm);
        createSearchRow(i, scope, searchTerm);
    }
    initializeBooleanWidgets();
}

function onMore(event)
{
    if(gTotalSearchTerms==1)
	gSearchLessButton .removeAttribute("disabled", "false");
    createSearchRow(gTotalSearchTerms++, gSearchScope, null);
}

function onLess(event)
{
    if (gTotalSearchTerms>1)
        removeSearchRow(--gTotalSearchTerms);
    if (gTotalSearchTerms==1)
        gSearchLessButton .setAttribute("disabled", "true");
}

// set scope on all visible searhattribute tags
function setSearchScope(scope) {
    dump("Setting search scope to " + scope + "\n");
    gSearchScope = scope;
    var searchTermElements = gSearchTermContainer.childNodes;
    if (!searchTermElements) return;
    dump("..on " + searchTermElements.length + " elements.\n");
    for (var i=0; i<searchTermElements.length; i++) {
        searchTermElements[i].searchattribute.searchScope = scope;
    }
}

function booleanChanged(event) {
    // when boolean changes, we have to update all the attributes on the
    // search terms

    var newBoolValue =
        (event.target.getAttribute("data") == "and") ? true : false;
    searchTermElements = gSearchTermContainer.childNodes;
    if (!searchTermElements) return;
    for (var i=0; i<searchTermElements.length; i++) {
        var searchTerm = searchTermElements[i];
        searchTerm.booleanAnd = newBoolValue;
    }
    dump("Boolean is now " + event.target.data + "\n");
}


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

    // now invalidate the newly created items because they've been inserted
    // into the document, and XBL bindings will be inserted in their place
    searchAttr = searchOp = searchVal = undefined;
    
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
    // the search term will initialize the searchTerm element, including
    // .booleanAnd
    if (searchTerm)
        searchTermElement.searchTerm = searchTerm;
    
    // here, we don't have a searchTerm, so it's probably a new element -
    // we'll initialize the .booleanAnd from the existing setting in
    // the UI
    else
        searchTermElement.booleanAnd = getBooleanAnd();

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
        if (treeItemRow.localName == "treeitem") break;
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

function getBooleanAnd()
{
    if (gSearchBooleanRadiogroup.selectedItem)
        return (gSearchBooleanRadiogroup.selectedItem.getAttribute("data") == "and") ? true : false;

    // default to false
    return false;
}

// save the search terms from the UI back to the actual search terms
// searchTerms: nsISupportsArray of terms
// termOwner:   object which can contain and create the terms
//              (will be unnecessary if we just make terms creatable
//               via XPCOM)
function saveSearchTerms(searchTerms, termOwner)
{
    var searchTermElements =
        gSearchTermContainer.childNodes;
    
    for (var i = 0; i<searchTermElements.length; i++) {
        try {
            dump("Saving search element " + i + "\n");
            var searchTerm = searchTermElements[i].searchTerm;
            if (searchTerm)
                searchTermElements[i].save();
            else {
                // need to create a new searchTerm, and somehow save it to that
                dump("Need to create searchterm " + i + "\n");
                searchTerm = termOwner.createTerm();
                searchTermElements[i].saveTo(searchTerm);
                termOwner.appendTerm(searchTerm);
            }
        } catch (ex) {

            dump("** Error: " + ex + "\n");
        }
    }

    // now remove the queued elements
    for (var i=0; i<gSearchRemovedTerms.length; i++) {
        // this is so nasty, we have to iterate through
        // because GetIndexOf is acting funny
        var searchTermSupports =
            gSearchRemovedTerms[i].QueryInterface(Components.interfaces.nsISupports);
        searchTerms.RemoveElement(searchTermSupports);
    }

}

function onReset(event)
{
    while (gTotalSearchTerms>0)
        removeSearchRow(--gTotalSearchTerms);
    onMore(event);
}
