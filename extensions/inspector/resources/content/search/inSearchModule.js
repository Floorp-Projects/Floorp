/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/***************************************************************
* inSearchModule -----------------------------------------------
*  Encapsulates an ISML module and exposes it to inSearchService.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
*   chrome://inspector/content/jsutil/rdf/RDFU.js
*   chrome://inspector/content/jsutil/rdf/RDFArray.js
*   chrome://inspector/content/jsutil/xul/inFormManager.js
*   chrome://inspector/content/search/xul/inSearchUtils.js
****************************************************************/

//////////// global variables /////////////////////

//////////// global constants ////////////////////

////////////////////////////////////////////////////////////////////////////
//// class inSearchModule

function inSearchModule(aBaseURL)
{
  this.mObservers = [];
  this.mBaseURL = aBaseURL;
}

inSearchModule.prototype = 
{
  mTitle: null,
  mBaseURL: null,
  mImpl: null,
  mDialogElementIds: null,
  mDialogURL: null,
  mContextMenuItems: null,
  mColumns: null,
  mColDelimiter: null,
  mNameSpace: null,
  
  mRDFArray: null,
  mResult: null,
  mObservers: null,

  mStartTime: null,
  mElapsed: 0,
  
  //////////////////////////////////////////////////////////////////////////
  //// Properties
  
  get searchService() { return this.mSearchService},
  set searchService(aVal) { this.mSearchService = aVal },
  
  get title() { return this.mTitle },
  get baseURL() { return this.mBaseURL },
  get defaultIconURL() { return this.mDefaultIconURL },

  get datasource() { return this.mRDFArray.datasource },
  get resultCount() { return this.mRDFArray.length },
  get progressPercent() { return this.mImpl.progressPercent },
  get progressText() { return this.mImp.progressText },
  get isPastMilestone() { return this.mImpl.isPastMilestone },
  get elapsed() { return this.mElapsed },
  
  //////////////////////////////////////////////////////////////////////////
  //// Initialization
  
  initFromElement: function(aSearchEl)
  {
    this.parseSearch(aSearchEl);
    
    if (this.mImpl.constructor)
      this.mImpl.constructor();
  },
  
  //////////////////////////////////////////////////////////////////////////
  //// Parser

  parseSearch: function(aSearchEl)
  {
    // get the title
    this.mTitle = aSearchEl.getAttribute("title");
    // get the default icon url
    this.mDefaultIconURL = aSearchEl.getAttribute("defaultIcon");
    // get the namespace
    var ns = aSearchEl.getAttribute("namespace")
    this.mNameSpace = ns ? ns : kInspectorNSURI;

    this.parseDialog(aSearchEl);
    this.parseContextMenu(aSearchEl);
    this.parseColumns(aSearchEl);
    this.parseImplementation(aSearchEl);
  },  

  parseDialog: function(aSearchEl)
  {
    var els = aSearchEl.getElementsByTagNameNS(kISMLNSURI, "dialog");
    if (els.length > 0) {
      var dialogEl = els[0];
      
      // get the array of dialog element ids
      var ids = dialogEl.getAttribute("elements");
      if (ids)
        this.mDialogElementIds = ids.split(",");

      // get the dialog url
      this.setDialogURL(dialogEl.getAttribute("href"));
      // get the dialog parameters
      this.mDialogResizable = (dialogEl.getAttribute("resizable") == "true") ? true : false;
    }
  },  
  
  parseContextMenu: function(aSearchEl)
  {
    var els = aSearchEl.getElementsByTagNameNS(kISMLNSURI, "contextmenu");
    if (els.length > 0) {
      var kids = els[0].childNodes;
      this.mContextMenu = [];
      for (var i = 0; i < kids.length; ++i) {
        this.mContextMenu[i] = kids[i].cloneNode(true);
      }      
    }
  },  

  parseColumns: function(aSearchEl)
  {
    // get the result columns
    var els = aSearchEl.getElementsByTagNameNS(kISMLNSURI, "columns");
    if (els.length > 0) {
      this.mColumns = [];
      var cols = els[0];
      this.mColDelimiter = cols.getAttribute("delimiter");
      
      var kids = cols.childNodes;
      var col, data;
      for (var i= 0; i < kids.length; ++i) {
        col = kids[i];
        if (col.nodeType == 1) { // ignore non-element nodes
          data = { 
            name: col.getAttribute("name"), 
            title: col.getAttribute("title"), 
            flex: col.getAttribute("flex"), 
            className: col.getAttribute("class"),
            copy: col.getAttribute("copy") == "true"
          };
          this.mColumns.push(data);
        }
      }
    }
  },  

  parseImplementation: function(aSearchEl)
  {
    this.mImpl = this.getDefaultImplementation();
    
    // get the implementation object
    var els = aSearchEl.getElementsByTagNameNS(kISMLNSURI, "implementation");
    if (els.length > 0) {
      var kids = aSearchEl.getElementsByTagNameNS(kISMLNSURI, "*");
      for (var i = 0; i < kids.length; i++) {
        if (kids[i].localName == "property")
          this.parseProperty(kids[i]);
        if (kids[i].localName == "method")
          this.parseMethod(kids[i]);
      }
    }
  },
  
  parseProperty: function(aPropEl)
  {
    var name = aPropEl.getAttribute("name");
    var fn = null;
    
    // look for a getter
    try {
      fn = this.getCodeTagFunction(aPropEl, "getter", null);
      if (fn)
        this.mImpl.__defineGetter__(name, fn);
    } catch (ex) {
      throw "### SYNTAX ERROR IN ISML GETTER \"" + name + "\" ###\n" + ex;
    }

    // look for a setter
    try {
      fn = this.getCodeTagFunction(aPropEl, "setter", ["val"]);
      if (fn)
        this.mImpl.__defineSetter__(name, fn);
    } catch (ex) {
      throw "### SYNTAX ERROR IN ISML SETTER \"" + name + "\" ###\n" + ex;
    }
  },
  
  parseMethod: function(aMethodEl)
  {
    var name = aMethodEl.getAttribute("name");
    var def = aMethodEl.getAttribute("defaultCommand") == "true";
    
    // get all the parameters
    var els = aMethodEl.getElementsByTagNameNS(kISMLNSURI, "parameter");
    var params = [];
    for (var i = 0; i < els.length; i++) {
      params[i] = els[i].getAttribute("name");
    }
    
    // get the body javascript and create the function
    try {
      var fn = this.getCodeTagFunction(aMethodEl, "body", params);
      this.mImpl[name] = fn;
      if (def)
        this.mImpl.__DefaultCmd__ = fn;
    } catch (ex) {
      throw "### SYNTAX ERROR IN ISML METHOD \"" + name + "\" ###\n" + ex;
    }    
  },
  
  getCodeTagFunction: function(aParent, aLocalName, aParams)
  {
    var els = aParent.getElementsByTagNameNS(kISMLNSURI, aLocalName);
    if (els.length) {
      var body = els[0];
      // try to determine where the code is located 
      var node = body.childNodes.length > 0 ? body.firstChild : body;
      return this.getJSFunction(aParams, node.nodeValue);
    }
    
    return null;
  },  
  
  getJSFunction: function(aParams, aCode)
  {
    var params = "";
    if (aParams) {
      for (var i = 0; i < aParams.length; i++) {
        params += aParams[i];
        if (i < aParams.length-1) params += ",";
      }
    }
   
    var js = "function(" + params + ") " + 
      "{" + 
        (aCode ? aCode : "") + 
      "}";
    
    var fn;
    eval("fn = " + js);
    return fn;
  },
  
  getDefaultImplementation: function()
  {
    return { module: this };
  },

  //////////////////////////////////////////////////////////////////////////
  //// Dialog box 
 
  openDialog: function()
  {
    window.openDialog(this.mDialogURL, "inSearchModule_dialog", 
      "chrome,modal,resizable="+this.mDialogResizable, this);
  },
  
  processDialog: function(aWindow)
  {
    var map = inFormManager.readWindow(aWindow, this.mDialogElementIds);
    this.implStartSearch(map);
  },

  //////////////////////////////////////////////////////////////////////////
  //// Searching

  startSearch: function()
  {
    if (this.mDialogURL) {
      this.openDialog();
    } else
      this.implStartSearch(null);  
  },
  
  implStartSearch: function(aMap)
  {
    this.mStartTime = new Date();
    this.mElapsed = 0;
    
    this.notifySearchStart();
    this.initDataSource();
    this.prepareForResult();
    this.mImpl.searchStart(aMap);    
  },
  
  stopSearch: function()
  {
    this.searchEnd();
  },
  
  //////////////////////////////////////////////////////////////////////////
  //// Result Returns

  setResultProperty: function(aAttr, aValue)
  {
    this.mResult[aAttr] = aValue;
  },
  
  searchResultReady: function()
  {
    this.mRDFArray.add(this.mResult);
    this.notifySearchResult();
    this.prepareForResult();
  },
  
  searchError: function(aMsg)
  {
  },
  
  searchEnd: function()
  {
    this.mElapsed = new Date() - this.mStartTime;
    this.notifySearchEnd();
  },

  //////////////////////////////////////////////////////////////////////////
  //// Columns

  get columnCount() { return this.mColumns.length },
  
  getColumn: function(aIndex) { return this.mColumns[aIndex] },
  getColumnName: function(aIndex) { return this.mColumns[aIndex].name },
  getColumnTitle: function(aIndex) { return this.mColumns[aIndex].title },
  getColumnClassName: function(aIndex) { return this.mColumns[aIndex].className },
  getColumnFlex: function(aIndex) { return this.mColumns[aIndex].flex },
 
  //////////////////////////////////////////////////////////////////////////
  //// RDF Datasource

  initDataSource: function()
  {
    this.mRDFArray = new RDFArray(this.mNameSpace, "inspector:searchResults", "results");
    this.mRDFArray.initialize();
  },
  
  getResultPropertyAt: function(aIndex, aProp)
  {
    return this.mRDFArray.get(aIndex, aProp);
  },
  
  getItemText: function(aIndex)
  {
    var cols = this.mColumns;
    var text = [];
    for (var i = 0; i < cols.length; ++i) {
      if (cols[i].copy) {
        text.push(this.getResultPropertyAt(aIndex, cols[i].name));
      }
    }
    return text.join(this.mColDelimiter);
  },

  //////////////////////////////////////////////////////////////////////////
  //// Context Menu
  
  installContextMenu: function(aMenu, aInsertionPoint, aDir)
  {
    if (this.mContextMenu) {
      aMenu._searchModule = this;
      var item;
      this.mMenuItems = [];
      if (this.mContextMenu.length == 0)
        aInsertionPoint.setAttribute("hide", "true");
      for (var i = 0; i < this.mContextMenu.length; ++i) {
        item = this.mContextMenu[i];
        this.mMenuItems.push(item);
        this.installSearchReference(item);
        if (aDir == inSearchService.INSERT_BEFORE)
          aMenu.insertBefore(item, aInsertionPoint);
        else {
         // NOT YET IMPLEMENTED
        }
      }
    }
  },
  
  uninstallContextMenu: function(aMenu, aInsertionPoint, aDir)
  {
    if (this.mContextMenu) {
      if (this.mContextMenu.length == 0)
        aInsertionPoint.setAttribute("hide", "false");
      // remove the menu items
      for (var i = 0; i < this.mContextMenu.length; ++i)
        aMenu.removeChild(this.mMenuItems[i]);
    }
  },

  installSearchReference: function(aItem)
  {
    if (aItem.nodeType == 1) {
      if (aItem.localName == "menuitem") {
        aItem.search = this.mImpl;
        for (var i = 0; i < aItem.childNodes.length; ++i)
          this.installSearchReference(aItem.childNodes[i]);
      }
    }
  },
  
  //////////////////////////////////////////////////////////////////////////
  //// Event Notification

  // NOTE TO SELF - this code could be cut down to nothing if you write a module
  // called "ObserverManager" to do the work for you

  addSearchObserver: function(aObserver)
  {
    this.mObservers.push(aObserver);
  },
  
  removeSearchObserver: function(aObserver)
  {
    var o;
    var obs = this.mObservers;
    for (var i = 0; i < obs.length; i++) {
      o = obs[i];
      if (o == aObserver) {
        obs.splice(i, 1);
        return;
      }
    }
  },

  notifySearchStart: function()
  {
    var o = this.mObservers;
    for (var i = 0; i < o.length; i++)
      o[i].onSearchStart(this);
  },
  
  notifySearchResult: function()
  {
    var o = this.mObservers;
    for (var i = 0; i < o.length; i++)
      o[i].onSearchResult(this);
  },

  notifySearchEnd: function(aResult)
  {
    var o = this.mObservers;
    for (var i = 0; i < o.length; i++)
      o[i].onSearchEnd(this, aResult);
  },

  notifySearchError: function(aMsg)
  {
    var o = this.mObservers;
    for (var i = 0; i < o.length; i++)
      o[i].onSearchError(this, aMsg);
  },

  //////////////////////////////////////////////////////////////////////////
  //// Uncategorized

  callDefaultCommand: function()
  {
    if (this.mImpl.__DefaultCmd__)
      this.mImpl.__DefaultCmd__();
  },
  
  prepareForResult: function()
  {
    this.mResult = { _icon: this.mDefaultIconURL };
  },

  setDialogURL: function(aURL)
  {
    this.mDialogURL = aURL;
    // This block below doesn't work for now because none of the local file implementations 
    // implement SetURL.  So, for now, the url in the search file MUST be absolute :(
    /* this.mDialogURL = aURL;
    var baseFile = inSearchUtils.createLocalFile(this.mBaseURL);
    try {
      baseFile.append(aURL);
    } catch (ex) {
      basefile = inSearchUtils.createLocalFile(aURL);      
    }
    var file = XPCU.QI(file, "nsIFile");
    this.mDialogURL = basefile.URL;
    */
  }  

};

//////////////////////////////////////////////////////////////////////////
//// Event Listeners

