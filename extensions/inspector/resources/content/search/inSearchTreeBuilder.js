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

