/*
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 */

/***************************************************************
* inSearchTreeBuilder ------------------------------------------
*  Utility for automatically building an rdf template and xul 
*  tree structure from a tabular data set.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
* chrome://inspector/content/jsutil/inTreeBuilder.js
****************************************************************/

//////////// global variables /////////////////////

//////////// global constants ////////////////////

////////////////////////////////////////////////////////////////////////////
//// class inSearchTreeBuilder

function inSearchTreeBuilder(aTree, aNameSpace, aArcName)
{
  this.mBuilder = new inTreeTableBuilder(aTree, aNameSpace, aArcName);
  aTree.setAttribute("ref", "inspector:searchResults");
  this.mBuilder.initialize();
}

inSearchTreeBuilder.prototype = 
{
  mCommited: false,
  mBuilder: null,
  mModule: null,
  mCommitted: false,
  
  get tree() { return this.mBuilder.tree },
  set tree(aVal) { this.mBuilder.tree = aVal },

  get isIconic() { return this.mBuilder.isIconic },
  set isIconic(aVal) { this.mBuilder.isIconic = aVal },
  
  get module() { return this.mModule },
  
  set module(aModule) 
  { 
    if (aModule != this.mModule) {
      if (this.mModule) {
        if (this.mCommitted)
          this.mBuilder.tree.database.RemoveDataSource(this.mModule.datasource);
        this.mCommitted = false;
        
        this.mModule.removeSearchObserver(this);
        this.mBuilder.reset();
      }
      
      this.mModule = aModule;

      if (aModule) {
        aModule.addSearchObserver(this);
        
        this.isIconic = aModule.defaultIconURL ? true : false;
      
        for (var i = 0; i < aModule.columnCount; i++)
          this.mBuilder.addColumn(aModule.getColumn(i)); 
  
        this.mBuilder.build();
      }
    }
  },

  getSelectedSearchIndex: function()
  {
    var tItem = this.mSearchTree.selectedItems[0];
    return this.mSearchTree.getIndexOfItem(tItem);
  },
  
  onSearchStart: function(aProcess)
  {
    if (this.mCommitted)
      this.mBuilder.tree.database.RemoveDataSource(this.mModule.datasource);
  },

  onSearchResult: function(aProcess)
  {
  },

  onSearchEnd: function(aProcess, aResult)
  {
    this.mBuilder.tree.database.AddDataSource(this.mModule.datasource);
    this.mBuilder.buildContent();
    this.mCommitted = true;
  },

  onSearchError: function(aProcess, aMessage)
  {
  },
  
  reset: function()
  {
    this.mBuilder.reset();
  },
  
  buildContent: function()
  {
    this.mBuilder.buildContent();
  }

};

