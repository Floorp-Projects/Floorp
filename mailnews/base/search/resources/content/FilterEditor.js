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
var nsIMsgSearchValidityManager = Components.interfaces.nsIMsgSearchValidityManager;

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
}

// set scope on all visible searhattribute tags
function setScope(scope) {
    var searchAttributes = document.getElementsByTagName("searchattribute");
    for (var i = 0; i<searchAttributes.length; i++) {
        searchAttributes[i].searchScope = scope;
    }
}


function scopeChanged(event)
{
    var menuitem = event.target;

    var searchattr = document.getElementById("searchAttr");
    try {
      searchattr.searchScope = menuitem.data;
    } catch (ex) {
        
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
    var filterName = document.getElementById("filterName");
    filterName.value = filter.filterName;

    var actionElement = document.getElementById("actionMenu");
    actionElement.selectedItem=actionElement.getElementsByAttribute("data", filter.action)[0];

    
    // now test by initializing the psuedo <searchterm>
    var searchTerm = document.getElementById("searchTerm");
    var scope = getScope(filter);
    var filterRowContainer = document.getElementById("filterTermList");
    var numTerms = filter.numTerms;
    for (var i=0; i<numTerms; i++) {
      var filterRow = createFilterRow(i);
      filterRowContainer.appendChild(filterRow);

      // now that it's been added to the document, we can initialize it.
      var filterTermObject = document.getElementById("searchTerm" + i);

      if (filterTermObject) {
          filterTermObject.searchScope = scope;
          var searchTerm =
              filter.searchTerms.QueryElementAt(i, Components.interfaces.nsIMsgSearchTerm);
          if (searchTerm)
              filterTermObject.searchTerm = searchTerm;

      }
      else {

          dump("Ack! Can't find searchTerm" + i + "!\n");
      }
    }

}

function createFilterRow(index)
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
    var searchtermContainer = document.getElementById("searchterms");
    var searchTerm = document.createElement("searchterm");

    // need to add it to the document before we can do anything about it
    searchTerm.id = "searchTerm" + index;
    searchtermContainer.appendChild(searchTerm);
    // now re-find the inserted element
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
    

    // now return the row
    return searchrow;
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

