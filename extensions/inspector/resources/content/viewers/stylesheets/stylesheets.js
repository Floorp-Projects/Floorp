/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/***************************************************************
* StylesheetsViewer --------------------------------------------
*  The viewer for the stylesheets loaded by a document.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
****************************************************************/

//////////// global variables /////////////////////

var viewer;

//////////// global constants ////////////////////

//////////////////////////////////////////////////

window.addEventListener("load", StylesheetsViewer_initialize, false);

function StylesheetsViewer_initialize()
{
  viewer = new StylesheetsViewer();
  viewer.initialize(parent.FrameExchange.receiveData(window));
}

////////////////////////////////////////////////////////////////////////////
//// class StylesheetsViewer

function StylesheetsViewer()
{
  this.mURL = window.location;
  this.mObsMan = new ObserverManager(this);

  this.mTree = document.getElementById("olStyleSheets");
  this.mOlBox = this.mTree.treeBoxObject;
}

StylesheetsViewer.prototype = 
{
  ////////////////////////////////////////////////////////////////////////////
  //// Initialization
  
  mSubject: null,
  mPane: null,
  mView: null,
  
  ////////////////////////////////////////////////////////////////////////////
  //// interface inIViewer

  get uid() { return "stylesheets"; },
  get pane() { return this.mPane; },
  get selection() { return this.mSelection; },

  get subject() { return this.mSubject; },
  set subject(aObject) 
  {
    this.mView = new StyleSheetsView(aObject);
    this.mOlBox.view = this.mView;
    this.mObsMan.dispatchEvent("subjectChange", { subject: aObject });
    this.mView.selection.select(0);
  },

  initialize: function(aPane)
  {
    this.mPane = aPane;
    aPane.notifyViewerReady(this);
  },

  destroy: function()
  {
    this.mOlBox.view = null;
  },

  isCommandEnabled: function(aCommand)
  {
    return false;
  },
  
  getCommand: function(aCommand)
  {
    return null;
  },
  
  ////////////////////////////////////////////////////////////////////////////
  //// event dispatching

  addObserver: function(aEvent, aObserver) { this.mObsMan.addObserver(aEvent, aObserver); },
  removeObserver: function(aEvent, aObserver) { this.mObsMan.removeObserver(aEvent, aObserver); },

  ////////////////////////////////////////////////////////////////////////////
  //// stuff
  
  onItemSelected: function()
  {
    var idx = this.mTree.currentIndex;
    this.mSelection = this.mView.getSheet(idx);
    this.mObsMan.dispatchEvent("selectionChange", { selection: this.mSelection } );
  }

};

////////////////////////////////////////////////////////////////////////////
//// StyleSheetsView

function StyleSheetsView(aDocument)
{
  this.mDocument = aDocument;
  this.mSheets = [];
  this.mLevels = [];
  this.mOpen = [];
  this.mChildCount = [];
  this.mRowCount = 0;
      
  var ss = aDocument.styleSheets;
  for (var i = 0; i < ss.length; ++i)
    this.insertSheet(ss[i], 0, -1);
}

StyleSheetsView.prototype = new inBaseTreeView();

StyleSheetsView.prototype.getSheet = 
function(aRow)
{
  return this.mSheets[aRow];
}

StyleSheetsView.prototype.insertSheet = 
function(aSheet, aLevel, aRow)
{
  var row = aRow < 0 ? this.mSheets.length : aRow;
  
  this.mSheets[row] = aSheet;
  this.mLevels[row] = aLevel;
  this.mOpen[row] = false;
  
  var count = 0;
  var rules = aSheet.cssRules;
  for (var i = 0; i < rules.length; ++i) {
    if (rules[i].type == CSSRule.IMPORT_RULE)
      ++count;
  }
  this.mChildCount[row] = count;
  ++this.mRowCount;
}

StyleSheetsView.prototype.shiftDataDown = 
function(aRow, aDiff)
{
  for (var i = this.mRowCount+aDiff-1; i >= aRow ; --i) {
    this.mSheets[i] = this.mSheets[i-aDiff];
    this.mLevels[i] = this.mLevels[i-aDiff];
    this.mOpen[i] = this.mOpen[i-aDiff];
    this.mChildCount[i] = this.mChildCount[i-aDiff];
  }
}

StyleSheetsView.prototype.shiftDataUp = 
function(aRow, aDiff)
{
  for (var i = aRow; i < this.mRowCount; ++i) {
    this.mSheets[i] = this.mSheets[i+aDiff];
    this.mLevels[i] = this.mLevels[i+aDiff];
    this.mOpen[i] = this.mOpen[i+aDiff];
    this.mChildCount[i] = this.mChildCount[i+aDiff];
  }
}

StyleSheetsView.prototype.getCellText = 
function(aRow, aCol) 
{
  if (aCol.id == "olcHref")
    return this.mSheets[aRow].href;
  else if (aCol.id == "olcRules")
    return this.mSheets[aRow].cssRules.length;
  return "";
}

StyleSheetsView.prototype.getLevel = 
function(aRow) 
{
  return this.mLevels[aRow];
}

StyleSheetsView.prototype.isContainer = 
function(aRow) 
{
  return this.mChildCount[aRow] > 0;
}

StyleSheetsView.prototype.isContainerEmpty = 
function(aRow) 
{
  return !this.isContainer(aRow);
}

StyleSheetsView.prototype.getParentIndex = 
function(aRow) 
{
  var baseLevel = this.mLevels[aRow];
  for (var i = aRow-1; i >= 0; --i) {
    if (this.mLevels[i] < baseLevel)
      return i;
  }
  return -1;
}

StyleSheetsView.prototype.hasNextSibling = 
function(aRow) 
{
  var baseLevel = this.mLevels[aRow];
  for (var i = aRow+1; i < this.mRowCount; ++i) {
    if (this.mLevels[i] < baseLevel)
      break;
    if (this.mLevels[i] == baseLevel)
      return true;
  }
  return false;
}

StyleSheetsView.prototype.isContainerOpen = 
function(aRow) 
{
  return this.mOpen[aRow];
}

StyleSheetsView.prototype.toggleOpenState = 
function(aRow) 
{
  var oldRowCount = this.mRowCount;
  if (this.mOpen[aRow]) {
    var baseLevel = this.mLevels[aRow];
    var count = 0;
    for (var i = aRow+1; i < this.mRowCount; ++i) {
      if (this.mLevels[i] <= baseLevel)
        break;
      ++count;
    }
    this.shiftDataUp(aRow+1, count);
    this.mRowCount -= count;
  } else {
    this.shiftDataDown(aRow+1, this.mChildCount[aRow]);
    
    var rules = this.mSheets[aRow].cssRules;
    for (changeCount = 0; changeCount < rules.length; ++changeCount) {
      if (rules[changeCount].type == CSSRule.IMPORT_RULE)
        this.insertSheet(rules[changeCount].styleSheet, this.mLevels[aRow]+1, aRow+changeCount+1);
    }
  }
  
  this.mOpen[aRow] = !this.mOpen[aRow];
  this.mTree.rowCountChanged(aRow+1, this.mRowCount - oldRowCount);
}

