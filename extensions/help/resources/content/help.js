/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * Contributor(s): 
 *    Original author: oeschger@netscape.com 
 *    amended by: Peter Wilson (added sidebar tabs) */
 
//-------- global variables
var helpBrowser;
var helpWindow;
var helpSearchPanel;
var emptySearch;
var emptySearchText
var emptySearchLink
var helpTocPanel;
var helpIndexPanel;
var helpGlossaryPanel;

// Namespaces
const NC = "http://home.netscape.com/NC-rdf#";
const SN = "rdf:http://www.w3.org/1999/02/22-rdf-syntax-ns#";
const XML = "http://www.w3.org/XML/1998/namespace#"
const MAX_LEVEL = 40; // maximum depth of recursion in search datasources.

// Resources
var RDF = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);
var RDF_ROOT = RDF.GetResource("urn:root");
var NC_PANELLIST = RDF.GetResource(NC + "panellist");
var NC_PANELID = RDF.GetResource(NC + "panelid");
var NC_EMPTY_SEARCH_TEXT = RDF.GetResource(NC + "emptysearchtext");
var NC_EMPTY_SEARCH_LINK = RDF.GetResource(NC + "emptysearchlink");
var NC_DATASOURCES = RDF.GetResource(NC + "datasources");
var NC_SUBHEADINGS = RDF.GetResource(NC + "subheadings");
var NC_NAME = RDF.GetResource(NC + "name");
var NC_CHILD = RDF.GetResource(NC + "child");
var NC_LINK = RDF.GetResource(NC + "link");
var NC_TITLE = RDF.GetResource(NC + "title");
var NC_BASE = RDF.GetResource(NC + "base"); 
var NC_DEFAULTTOPIC = RDF.GetResource(NC + "defaulttopic"); 

var RDFCUtils = Components.classes["@mozilla.org/rdf/container-utils;1"].getService().
   QueryInterface(Components.interfaces.nsIRDFContainerUtils);
var RDFContainer = Components.classes["@mozilla.org/rdf/container;1"].getService(Components.interfaces.nsIRDFContainer);
var CONSOLE_SERVICE = Components.classes['@mozilla.org/consoleservice;1'].getService(Components.interfaces.nsIConsoleService);
            
var urnID = 0;
var RE;

var helpFileURI;
var helpFileDS;
// Set from nc:base attribute on help rdf file. It may be used for prefix reduction on all links within
// the current help set.
var helpBaseURI;

const defaultHelpFile = "chrome://help/locale/mozillahelp.rdf";
// Set from nc:defaulttopic. It is used when the requested uri has no topic specified. 
var defaultTopic = "welcome"; 
var searchDatasources = "rdf:null";
var searchDS = null;

const NSRESULT_RDF_SYNTAX_ERROR = 0x804e03f7; 

// This function is called by dialogs/windows that want to display context-sensitive help
// These dialogs/windows should include the script chrome://help/content/contextHelp.js
function displayTopic(topic) {
  if (!topic)
    topic = defaultTopic;
  var uri = getLink(topic);
  if (!uri) // Topic not found - revert to default.
      uri = getLink(defaultTopic); 
  loadURI(uri);
}

// Initialize the Help window
function init() {
  //cache panel references.
  helpWindow = document.getElementById("help");
  helpSearchPanel = document.getElementById("help-search-panel");
  helpTocPanel = document.getElementById("help-toc-tree");
  helpIndexPanel = document.getElementById("help-index-tree");
  helpGlossaryPanel = document.getElementById("help-glossary-tree");
  helpBrowser = document.getElementById("help-content");

  var URI = normalizeURI(decodeURIComponent(window.location.search));
  helpFileURI = URI.helpFileURI;
  var helpTopic = URI.topic;
  helpBaseURI = helpFileURI.substring(0, helpFileURI.lastIndexOf("/")+1); // trailing "/" included.

  loadHelpRDF();

  displayTopic(helpTopic);  

  // move to right end of screen
  var width = document.documentElement.getAttribute("width");
  var height = document.documentElement.getAttribute("height");
  window.moveTo(screen.availWidth-width, (screen.availHeight-height)/2);

  var sessionHistory =  Components.classes["@mozilla.org/browser/shistory;1"]
                  .createInstance(Components.interfaces.nsISHistory);

  getWebNavigation().sessionHistory = sessionHistory;
  window.XULBrowserWindow = new nsHelpStatusHandler();
  // hook up UI through progress listener
  var interfaceRequestor = helpBrowser.docShell.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
  var webProgress = interfaceRequestor.getInterface(Components.interfaces.nsIWebProgress);
  webProgress.addProgressListener(window.XULBrowserWindow, Components.interfaces.nsIWebProgress.NOTIFY_ALL);
}

function normalizeURI(uri) {
  // uri in format [uri of help rdf file][?initial topic]
  // if the whole uri or help file uri is omitted then the default help is assumed.
  // unpack uri
  var URI = {};
  URI.helpFileURI = defaultHelpFile;
  URI.topic = null;
  // Case: No help uri at all.
  if (uri) {
    // remove leading ?
    if (uri.substr(0,1) == "?") 
      uri = uri.substr(1);
    var i = uri.indexOf("?");
    // Case: Full uri with topic.
    if ( i != -1) {
        URI.helpFileURI = uri.substr(0,i);
        URI.topic = uri.substr(i+1);
    }
    else {
      // Case: uri with no topic.
      if (uri.substr(0,7) == "chrome:") 
        URI.helpFileURI = uri;
      else {
        // Case: uri with topic only.
        URI.topic = uri;
      }  
    }
  }  
  URI.uri = URI.helpFileURI + ((URI.topic)? "?" + URI.topic : "");
  return URI;
}

function loadHelpRDF() {
  if (!helpFileDS) {
    try {
      helpFileDS = RDF.GetDataSourceBlocking(helpFileURI);
    }
    catch (e if (e.result == NSRESULT_RDF_SYNTAX_ERROR)) {
      log("Help file: " + helpFileURI + " contains a syntax error.");
    }
    catch (e) {
      log("Help file: " + helpFileURI + " was not found.");
    }
    try {
      helpWindow.setAttribute("title", getAttribute(helpFileDS, RDF_ROOT, NC_TITLE, ""));
      helpBaseURI = getAttribute(helpFileDS, RDF_ROOT, NC_BASE, helpBaseURI);
      defaultTopic = getAttribute(helpFileDS, RDF_ROOT, NC_DEFAULTTOPIC, "welcome");

      var panelDefs = helpFileDS.GetTarget(RDF_ROOT, NC_PANELLIST, true);      
      RDFContainer.Init(helpFileDS, panelDefs);
      var iterator = RDFContainer.GetElements();
        while (iterator.hasMoreElements()) {
        var panelDef = iterator.getNext();
        var panelID = getAttribute(helpFileDS, panelDef, NC_PANELID, null);        

        var datasources = getAttribute(helpFileDS, panelDef, NC_DATASOURCES, "rdf:none");
        datasources = normalizeLinks(helpBaseURI, datasources);
        // cache additional datsources to augment search datasources.
        if (panelID == "search") {
	       emptySearchText = getAttribute(helpFileDS, panelDef, NC_EMPTY_SEARCH_TEXT, null) || "No search items found." ;        
	       emptySearchLink = getAttribute(helpFileDS, panelDef, NC_EMPTY_SEARCH_LINK, null) || "about:blank";        
          searchDatasources = datasources;
          datasources = "rdf:null"; // but don't try to display them yet!
        }  

        // cache toc datasources for use by ID lookup.
        var tree = document.getElementById("help-" + panelID + "-tree");
        tree.setAttribute("datasources", datasources);
        //if (panelID == "toc") {
          if (tree.database) {
            loadDatabases(tree.database, datasources);
            tree.builder.rebuild();
          }
        //}
      }  
    }
    catch (e) {
      log(e + "");      
    }
  }
}
function loadDatabases(compositeDatabase, datasources) {
  var ds = datasources.split(/\s+/);
  for (var i=0; i < ds.length; ++i) {
    if (ds[i] == "rdf:null" || ds[i] == "")
      continue;
    try {  
      // we need blocking here to ensure the database is loaded so getLink(topic) works.
      var datasource = RDF.GetDataSourceBlocking(ds[i]);
      if (datasource)  
        compositeDatabase.AddDataSource(datasource);
    }
    catch (e) {
      log("Datasource: " + ds[i] + " was not found.");
    }
  }
}

// prepend helpBaseURI to list of space separated links if the don't start with "chrome:"
function normalizeLinks(helpBaseURI, links) {
  if (!helpBaseURI)
    return links;
  var ls = links.split(/\s+/);
  if (ls.length == 0)
    return links;
  for (var i=0; i < ls.length; ++i) {
    if (ls[i] == "")
      continue;
    if (ls[i].substr(0,7) != "chrome:" && ls[i].substr(0,4) != "rdf:") 
      ls[i] = helpBaseURI + ls[i];
  }
  return ls.join(" ");  
}

function getLink(ID) {
  if (!ID)
    return null;
  // Note resources are stored in fileURL#ID format.
  // We have one possible source for an ID for each datasource in the composite datasource.
  // The first ID which matches is returned.
  var tocTree = document.getElementById("help-toc-tree");
    tocDS = tocTree.database;
    if (tocDS == null)
      return null;
    var tocDatasources = tocTree.getAttribute("datasources");
  var ds = tocDatasources.split(/\s+/);
  for (var i=0; i < ds.length; ++i) {
    if (ds[i] == "rdf:null" || ds[i] == "")
      continue;
    try {
      var rdfID = ds[i] + "#" + ID;
      var resource = RDF.GetResource(rdfID);
      if (resource) {
        var link = tocDS.GetTarget(resource, NC_LINK, true);
        if (link) {
          link = link.QueryInterface(Components.interfaces.nsIRDFLiteral);
          if (link) 
            return link.Value;
          else  
            return null;
        }  
      }
    }
    catch (e) { log(rdfID + " " + e);}
  }
  return null;
}

// Called by contextHelp.js to determine if this window is displaying the requested help file.
function getHelpFileURI() {
  return helpFileURI;
}


function getWebNavigation()
{
  return helpBrowser.webNavigation;
}

function loadURI(uri)
{
  if (uri.substr(0,7) != "chrome:")
    uri = helpBaseURI + uri;
  const nsIWebNavigation = Components.interfaces.nsIWebNavigation;
  getWebNavigation().loadURI(uri, nsIWebNavigation.LOAD_FLAGS_NONE, null, null, null);
}

function goBack()
{
  var webNavigation = getWebNavigation();
  if (webNavigation.canGoBack)
    webNavigation.goBack();
}

function goForward()
{
  var webNavigation = getWebNavigation();
  if (webNavigation.canGoForward)
    webNavigation.goForward();
}

function goHome() {
  // load "Welcome" page
  displayTopic(defaultTopic);
}

function print()
{
  try {
    _content.print();
  } catch (e) {
  }
}

function createBackMenu(event)
{
  return FillHistoryMenu(event.target, "back");
}

function createForwardMenu(event)
{
  return FillHistoryMenu(event.target, "forward");
}

function gotoHistoryIndex(aEvent)
{
  var index = aEvent.target.getAttribute("index");
  if (!index)
    return false;
  try {
    getWebNavigation().gotoIndex(index);
  }
  catch(ex) {
    return false;
  }
  return true;
}

function BrowserBack()
{
  try {
    getWebNavigation().goBack();
  }
  catch(ex) {
  }
  UpdateBackForwardButtons();
}

function BrowserForward()
{
  try {
    getWebNavigation().goForward();
  }
  catch(ex) {
  }
  UpdateBackForwardButtons();
}

function nsHelpStatusHandler()
{
}

nsHelpStatusHandler.prototype =
{
  onStateChange : function(aWebProgress, aRequest, aStateFlags, aStatus) {},
  onProgressChange : function(aWebProgress, aRequest, aCurSelfProgress,
                              aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress) {},
  onSecurityChange : function(aWebProgress, aRequest, state) {},
  onLocationChange : function(aWebProgress, aRequest, aLocation)
  {
    UpdateBackForwardButtons();
  },
  QueryInterface : function(aIID)
  {
    if (aIID.equals(Components.interfaces.nsIWebProgressListener) ||
      aIID.equals(Components.interfaces.nsISupportsWeakReference) ||
      aIID.equals(Components.interfaces.nsIXULBrowserWindow) ||
      aIID.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_NOINTERFACE;
  },
  setJSStatus : function(status) {},
  setJSDefaultStatus : function(status) {},
  setOverLink : function(link) {}
}

function UpdateBackForwardButtons()
{
  var backBroadcaster = document.getElementById("canGoBack");
  var forwardBroadcaster = document.getElementById("canGoForward");
  var webNavigation = getWebNavigation();

  // Avoid setting attributes on broadcasters if the value hasn't changed!
  // Remember, guys, setting attributes on elements is expensive!  They
  // get inherited into anonymous content, broadcast to other widgets, etc.!
  // Don't do it if the value hasn't changed! - dwh

  var backDisabled = (backBroadcaster.getAttribute("disabled") == "true");
  var forwardDisabled = (forwardBroadcaster.getAttribute("disabled") == "true");

  if (backDisabled == webNavigation.canGoBack)
    backBroadcaster.setAttribute("disabled", !backDisabled);
  
  if (forwardDisabled == webNavigation.canGoForward)
    forwardBroadcaster.setAttribute("disabled", !forwardDisabled);
}

function find(again, reverse)
{
  var focusedWindow = document.commandDispatcher.focusedWindow;
  if (!focusedWindow || focusedWindow == window)
    focusedWindow = window._content;
  if (again)
    findAgainInPage(helpBrowser, window._content, focusedWindow, reverse);
  else
    findInPage(helpBrowser, window._content, focusedWindow)
}

function getMarkupDocumentViewer()
{
  return helpBrowser.markupDocumentViewer;
}

function BrowserReload()
{
  const reloadFlags = Components.interfaces.nsIWebNavigation.LOAD_FLAGS_NONE;
  return BrowserReloadWithFlags(reloadFlags);
}

function BrowserReloadWithFlags(reloadFlags)
{
   try {
     /* Need to get SessionHistory from docshell so that
      * reload on framed pages will work right. This 
      * method should not be used for the context menu item "Reload frame".
      * "Reload frame" should directly call into docshell as it does right now
      */
     var sh = getWebNavigation().sessionHistory;
     var webNav = sh.QueryInterface(Components.interfaces.nsIWebNavigation);      
     webNav.reload(reloadFlags);
   }
   catch(ex) {
   }
 }

 // doc=null for regular page info, doc=owner document for frame info 
 function BrowserPageInfo(doc)
 {
   window.openDialog("chrome://navigator/content/pageInfo.xul",
                     "_blank",
                     "chrome,dialog=no",
                     doc);
}

function BrowserViewSource()
{
  var focusedWindow = document.commandDispatcher.focusedWindow;
  if (focusedWindow == window)
    focusedWindow = _content;

  if (focusedWindow)
    var docCharset = "charset=" + focusedWindow.document.characterSet;

  BrowserViewSourceOfURL(_content.location, docCharset);
}

function BrowserViewSourceOfURL(url, charset)
{
  // try to open a view-source window while inheriting the charset (if any)
  openDialog("chrome://navigator/content/viewSource.xul",
             "_blank",
             "scrollbars,resizable,chrome,dialog=no",
             url, charset);
}

//Show the selected sidebar panel
function showPanel(panelId) {
  helpSearchPanel.setAttribute("hidden", "true");
  helpTocPanel.setAttribute("hidden", "true");
  helpIndexPanel.setAttribute("hidden", "true");
  helpGlossaryPanel.setAttribute("hidden", "true");
  var thePanel = document.getElementById(panelId);
  thePanel.setAttribute("hidden","false");
}

function onselect_loadURI(tree, columnName) {
  try {
    var row = tree.treeBoxObject.view.selection.currentIndex;
    var properties = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
    tree.treeBoxObject.view.getCellProperties(row, columnName, properties);
    if (!properties) return;
    var uri = getPropertyValue(properties, "link-");
    if (uri)
      loadURI(uri);
  }
  catch (e) {}// when switching between tabs a spurious row number is returned.
}

/** Search properties nsISupportsArray for an nsIAtom which starts with the given property name. **/
function getPropertyValue(properties, propName) {
  for (var i=0; i< properties.Count(); ++i) {
    var atom = properties.GetElementAt(i).QueryInterface(Components.interfaces.nsIAtom);
    var atomValue = atom.GetUnicode();
    if (atomValue.substr(0, propName.length) == propName)
      return atomValue.substr(propName.length);
  }
  return null;
}

function doFind() {
  var searchTree = document.getElementById("help-search-tree");
  var findText = document.getElementById("findText");

  // clear any previous results.
  clearDatabases(searchTree.database);

  // split search string into separate terms and compile into regexp's
  RE = findText.value.split(/\s+/);
  for (var i=0; i < RE.length; ++i) {
    if (RE[i] == "")
      continue;
    if (RE[i].length > 3) {
        RE[i] = new RegExp(RE[i].substring(0, RE[i].length-1) +"\w?", "i");
    } else {
        RE[i] = new RegExp(RE[i], "i");
    }

  }
 emptySearch = true; 	
  // search TOC
  var resultsDS =  Components.classes["@mozilla.org/rdf/datasource;1?name=in-memory-datasource"].createInstance(Components.interfaces.nsIRDFDataSource);
  var tree = document.getElementById("help-toc-tree");
  var sourceDS = tree.database;
  doFindOnDatasource(resultsDS, sourceDS, RDF_ROOT, 0);

  // search additional search datasources                                       
  if (searchDatasources != "rdf:null") {
    if (!searchDS)
      searchDS = loadCompositeDS(searchDatasources);
    doFindOnDatasource(resultsDS, searchDS, RDF_ROOT, 0);
  }

  // search glossary.
  tree = document.getElementById("help-glossary-tree");
  sourceDS = tree.database;
  if (!sourceDS) // If the glossary has never been displayed this will be null (sigh!).
    sourceDS = loadCompositeDS(tree.datasources);
  doFindOnDatasource(resultsDS, sourceDS, RDF_ROOT, 0);
  
  if (emptySearch)
		assertSearchEmpty(resultsDS);
  // Add the datasource to the search tree
  searchTree.database.AddDataSource(resultsDS);
  searchTree.builder.rebuild();
}

function doEnabling() {
  var findButton = document.getElementById("findButton");
  var findTextbox = document.getElementById("findText");
  findButton.disabled = !findTextbox.value;
}

function clearDatabases(compositeDataSource) {
  var enumDS = compositeDataSource.GetDataSources()
  while (enumDS.hasMoreElements()) {
    var ds = enumDS.getNext();
        compositeDataSource.RemoveDataSource(ds);
  }
}

function doFindOnDatasource(resultsDS, sourceDS, resource, level) {
  if (level > MAX_LEVEL) {
    try {
      log("Recursive reference to resource: " + resource.Value + ".");
    }
    catch (e) {
      log("Recursive reference to unknown resource.");
    }
    return;
  }
  // find all SUBHEADING children of current resource.
  var targets = sourceDS.GetTargets(resource, NC_SUBHEADINGS, true);
  while (targets.hasMoreElements()) {
      var target = targets.getNext();
      target = target.QueryInterface(Components.interfaces.nsIRDFResource);
        // The first child of a rdf:subheading should (must) be a rdf:seq.
        // Should we test for a SEQ here?
    doFindOnSeq(resultsDS, sourceDS, target, level+1);       
  }  
}

function doFindOnSeq(resultsDS, sourceDS, resource, level) {
  // load up an RDFContainer so we can access the contents of the current rdf:seq.    
    RDFContainer.Init(sourceDS, resource);
    var targets = RDFContainer.GetElements();
    while (targets.hasMoreElements()) {
    var target = targets.getNext();
        target = target.QueryInterface(Components.interfaces.nsIRDFResource);
        var name = sourceDS.GetTarget(target, NC_NAME, true);
        name = name.QueryInterface(Components.interfaces.nsIRDFLiteral);
        
        if (isMatch(name.Value)) {
          // we have found a search entry - add it to the results datasource.
          
          // Get URL of html for this entry.
      var link = sourceDS.GetTarget(target, NC_LINK, true);
      link = link.QueryInterface(Components.interfaces.nsIRDFLiteral);        

      urnID++;
      resultsDS.Assert(RDF_ROOT,
             RDF.GetResource("http://home.netscape.com/NC-rdf#child"),
             RDF.GetResource("urn:" + urnID),
             true);
      resultsDS.Assert(RDF.GetResource("urn:" + urnID),
             RDF.GetResource("http://home.netscape.com/NC-rdf#name"),
             name,
             true);
      resultsDS.Assert(RDF.GetResource("urn:" + urnID),
             RDF.GetResource("http://home.netscape.com/NC-rdf#link"),
             link,
             true);
  		emptySearch = false; 	
             
    }
    // process any nested rdf:seq elements.
    doFindOnDatasource(resultsDS, sourceDS, target, level+1);       
    }  
}
function assertSearchEmpty(resultsDS) {
	var resSearchEmpty = RDF.GetResource("urn:emptySearch");
	resultsDS.Assert(RDF_ROOT,
			 NC_CHILD,
			 resSearchEmpty,
			 true);
	resultsDS.Assert(resSearchEmpty,
			 NC_NAME,
			 RDF.GetLiteral(emptySearchText),
			 true);
	resultsDS.Assert(resSearchEmpty,
			 NC_LINK,
			 RDF.GetLiteral(emptySearchLink),
			 true);
}

function isMatch(text) {
  for (var i=0; i < RE.length; ++i ) {
    if (!RE[i].test(text))
      return false;
  }
  return true;
}
function loadCompositeDS(datasources) {
  // We can't search on each individual datasource's - only the aggregate (for each sidebar tab)
  // has the appropriate structure.
  var compositeDS =  Components.classes["@mozilla.org/rdf/datasource;1?name=composite-datasource"]
      .createInstance(Components.interfaces.nsIRDFCompositeDataSource);
  
  var ds = datasources.split(/\s+/);
  for (var i=0; i < ds.length; ++i) {
    if (ds[i] == "rdf:null" || ds[i] == "")
      continue;
    try {  
      // we need blocking here to ensure the database is loaded.
      var sourceDS = RDF.GetDataSourceBlocking(ds[i]);
      compositeDS.AddDataSource(sourceDS);
    }
    catch (e) {
      log("Datasource: " + ds[i] + " was not found.");
    }
  }
  return compositeDS;
}

function getAttribute(datasource, resource, attributeResourceName, defaultValue) {
  var literal = datasource.GetTarget(resource, attributeResourceName, true);
  if (!literal)
    return defaultValue;
  return getLiteralValue(literal, defaultValue);  
}

function getLiteralValue(literal, defaultValue) {
  if (literal) {
      literal = literal.QueryInterface(Components.interfaces.nsIRDFLiteral);
      if (literal)
        return literal.Value;
  }
  if (defaultValue)
    return defaultValue;
  return null;
}
// Write debug string to javascript console.
function log(aText) {
  CONSOLE_SERVICE.logStringMessage(aText);
}


//INDEX OPENING FUNCTION -- called in oncommand for index pane
// iterate over all the items in the outliner;
// open the ones at the top-level (i.e., expose the headings underneath
// the letters in the list.
function displayIndex() {
  var treeview = helpIndexPanel.view;
  var i = treeview.rowCount;
  while (i--)
    if (!treeview.getLevel(i) && !treeview.isContainerOpen(i))
      treeview.toggleOpenState(i);
}

