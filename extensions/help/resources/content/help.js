/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ian Oeschger <oeschger@brownhen.com> (Original Author)
 *   Peter Wilson (added sidebar tabs)
 *   R.J. Keller <rlk@trfenv.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
const RDF = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);
const RDF_ROOT = RDF.GetResource("urn:root");
const NC_PANELLIST = RDF.GetResource(NC + "panellist");
const NC_PANELID = RDF.GetResource(NC + "panelid");
const NC_EMPTY_SEARCH_TEXT = RDF.GetResource(NC + "emptysearchtext");
const NC_EMPTY_SEARCH_LINK = RDF.GetResource(NC + "emptysearchlink");
const NC_DATASOURCES = RDF.GetResource(NC + "datasources");
const NC_SUBHEADINGS = RDF.GetResource(NC + "subheadings");
const NC_NAME = RDF.GetResource(NC + "name");
const NC_CHILD = RDF.GetResource(NC + "child");
const NC_LINK = RDF.GetResource(NC + "link");
const NC_TITLE = RDF.GetResource(NC + "title");
const NC_BASE = RDF.GetResource(NC + "base"); 
const NC_DEFAULTTOPIC = RDF.GetResource(NC + "defaulttopic"); 

const RDFCUtils = Components.classes["@mozilla.org/rdf/container-utils;1"].getService(Components.interfaces.nsIRDFContainerUtils);
var RDFContainer = Components.classes["@mozilla.org/rdf/container;1"].createInstance(Components.interfaces.nsIRDFContainer);
const CONSOLE_SERVICE = Components.classes['@mozilla.org/consoleservice;1'].getService(Components.interfaces.nsIConsoleService);
            
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
  // Use default topic if topic is not specified.
  if (!topic)
    topic = defaultTopic;

  // Get the help page to open.
  var uri = getLink(topic);

  // Use default topic if specified topic is not found.
  if (!uri) // Topic not found - revert to default.
    uri = getLink(defaultTopic);
  loadURI(uri);
}

// Initialize the Help window
function init() {
  //cache panel references.
  helpWindow = document.getElementById("help");
  helpSearchPanel = document.getElementById("help-search-panel");
  helpTocPanel = document.getElementById("help-toc-panel");
  helpIndexPanel = document.getElementById("help-index-panel");
  helpGlossaryPanel = document.getElementById("help-glossary-panel");
  helpBrowser = document.getElementById("help-content");

  // Get the help content pack, base URL, and help topic
  var helpTopic = defaultTopic;
  if ("arguments" in window && window.arguments[0] instanceof Components.interfaces.nsIDialogParamBlock) {
    helpFileURI = window.arguments[0].GetString(0);
    helpBaseURI = helpFileURI.substring(0, helpFileURI.lastIndexOf("/")+1); // trailing "/" included.
    helpTopic = window.arguments[0].GetString(1);
  }

  loadHelpRDF();

  displayTopic(helpTopic);

  // Initalize History.
  var sessionHistory =  Components.classes["@mozilla.org/browser/shistory;1"]
                                  .createInstance(Components.interfaces.nsISHistory);

  window.XULBrowserWindow = new nsHelpStatusHandler();

  //Start the status handler.
  window.XULBrowserWindow.init();

  // Hook up UI through Progress Listener
  const interfaceRequestor = helpBrowser.docShell.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
  const webProgress = interfaceRequestor.getInterface(Components.interfaces.nsIWebProgress);
  webProgress.addProgressListener(window.XULBrowserWindow, Components.interfaces.nsIWebProgress.NOTIFY_ALL);

  //Always show the Table of Contents sidebar at startup.
  showPanel('help-toc');
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
        var tree = document.getElementById("help-" + panelID + "-panel");
        loadDatabasesBlocking(datasources);
        tree.setAttribute("datasources", datasources);
      }  
    }
    catch (e) {
      log(e + "");      
    }
  }
}

function loadDatabasesBlocking(datasources) {
  var ds = datasources.split(/\s+/);
  for (var i=0; i < ds.length; ++i) {
    if (ds[i] == "rdf:null" || ds[i] == "")
      continue;
    try {  
      // we need blocking here to ensure the database is loaded so getLink(topic) works.
      var datasource = RDF.GetDataSourceBlocking(ds[i]);
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
  var tocTree = document.getElementById("help-toc-panel");
  var tocDS = tocTree.database;
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

function nsHelpStatusHandler()
{
}

nsHelpStatusHandler.prototype =
{
  onStateChange : function(aWebProgress, aRequest, aStateFlags, aStatus)
  {
    const nsIWebProgressListener = Components.interfaces.nsIWebProgressListener;

    // Turn on the throbber.
    if (aStateFlags & nsIWebProgressListener.STATE_START)
      this.throbberElement.setAttribute("busy", "true");
    else if (aStateFlags & nsIWebProgressListener.STATE_STOP)
      this.throbberElement.removeAttribute("busy");
  },
  onStatusChange : function(aWebProgress, aRequest, aStateFlags, aStatus) {},
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

  init : function()
  {
    this.throbberElement = document.getElementById("navigator-throbber");
  },

  destroy : function()
  {
    //this is needed to avoid memory leaks, see bug 60729
    this.throbberElement = null;
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

var gFindInstData;
function getFindInstData()
{
  if (!gFindInstData) {
    gFindInstData = new nsFindInstData();
    gFindInstData.browser = helpBrowser;
    // defaults for rootSearchWindow and currentSearchWindow are fine here
  }
  return gFindInstData;
}

function find(again, reverse)
{
  if (again)
    findAgainInPage(getFindInstData(), reverse);
  else
    findInPage(getFindInstData())
}

function getMarkupDocumentViewer()
{
  return helpBrowser.markupDocumentViewer;
}

//Show the selected sidebar panel
function showPanel(panelId) {
  //hide other sidebar panels and show the panel name taken in from panelID.
  helpSearchPanel.setAttribute("hidden", "true");
  helpTocPanel.setAttribute("hidden", "true");
  helpIndexPanel.setAttribute("hidden", "true");
  helpGlossaryPanel.setAttribute("hidden", "true");
  var thePanel = document.getElementById(panelId + "-panel");
  thePanel.setAttribute("hidden","false");

  //remove the selected style from the previous panel selected.
  document.getElementById("help-glossary-btn").removeAttribute("selected");
  document.getElementById("help-index-btn").removeAttribute("selected");
  document.getElementById("help-search-btn").removeAttribute("selected");
  document.getElementById("help-toc-btn").removeAttribute("selected");

  //add the selected style to the correct panel.
  var theButton = document.getElementById(panelId + "-btn");
  theButton.setAttribute("selected", "true");
}

function onselect_loadURI(tree) {
  var row = tree.currentIndex;
  if (row >= 0) {
    var resource = tree.view.getResourceAtIndex(row);
    var link = tree.database.GetTarget(resource, NC_LINK, true);
    if (link instanceof Components.interfaces.nsIRDFLiteral)
      loadURI(link.Value);
  }
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
    RE[i] = new RegExp(RE[i], "i");
  }
 emptySearch = true;
  // search TOC
  var resultsDS =  Components.classes["@mozilla.org/rdf/datasource;1?name=in-memory-datasource"].createInstance(Components.interfaces.nsIRDFDataSource);
  var tree = document.getElementById("help-toc-panel");
  var sourceDS = tree.database;
  doFindOnDatasource(resultsDS, sourceDS, RDF_ROOT, 0);

  // search additional search datasources
  if (searchDatasources != "rdf:null") {
    if (!searchDS)
      searchDS = loadCompositeDS(searchDatasources);
    doFindOnDatasource(resultsDS, searchDS, RDF_ROOT, 0);
  }

  // search index.
  tree = document.getElementById("help-index-panel");
  sourceDS = tree.database;
  if (!sourceDS) // If the index has never been displayed this will be null.
    sourceDS = loadCompositeDS(tree.datasources);
  doFindOnDatasource(resultsDS, sourceDS, RDF_ROOT, 0);

  // search glossary.
  tree = document.getElementById("help-glossary-panel");
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

// Shows the panel relative to the currently selected panel.
// Takes a boolean parameter - if true it will show the next panel, 
// otherwise it will show the previous panel.
function showRelativePanel(goForward) {
  var selectedIndex = -1;
  var sidebarBox = document.getElementById("helpsidebar-box");
  var sidebarButtons = new Array();
  for (var i = 0; i < sidebarBox.childNodes.length; i++) {
    var btn = sidebarBox.childNodes[i];
    if (btn.nodeName == "button") {
      if (btn.getAttribute("selected") == "true")
        selectedIndex = sidebarButtons.length;
      sidebarButtons.push(btn);
    }
  }
  if (selectedIndex == -1)
    return;
  selectedIndex += goForward ? 1 : -1;
  if (selectedIndex >= sidebarButtons.length)
    selectedIndex = 0;
  else if (selectedIndex < 0)
    selectedIndex = sidebarButtons.length - 1;
  sidebarButtons[selectedIndex].doCommand();
}
