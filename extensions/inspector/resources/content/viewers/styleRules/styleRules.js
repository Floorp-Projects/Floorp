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
* StyleRulesViewer --------------------------------------------
*  The viewer for CSS style rules that apply to a DOM element.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/util.js
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
*   chrome://inspector/content/jsutil/rdf/RDFU.js
****************************************************************/

//////////// global variables /////////////////////

var viewer;

//////////// global constants ////////////////////

var kCSSRuleDataSourceIID = "@mozilla.org/rdf/datasource;1?name=Inspector_CSSRules";
var kCSSDecDataSourceIID  = "@mozilla.org/rdf/datasource;1?name=Inspector_CSSDec";

var nsEventStateUnspecificed = 0;
var nsEventStateActive = 1;
var nsEventStateFocus = 2;
var nsEventStateHover = 4;
var nsEventStateDragOver = 8;

/////////////////////////////////////////////////

window.addEventListener("load", StyleRulesViewer_initialize, false);

function StyleRulesViewer_initialize()
{
  viewer = new StyleRulesViewer();
  viewer.initialize(parent.FrameExchange.receiveData(window));
}

////////////////////////////////////////////////////////////////////////////
//// class StyleRulesViewer

function StyleRulesViewer() // implements inIViewer
{
  this.mObsMan = new ObserverManager();
  
  this.mURL = window.location;
  this.mRuleTree = document.getElementById("olStyleRules");
  this.mRuleBoxObject = this.mRuleTree.treeBoxObject;
  this.mPropsTree = document.getElementById("olStyleProps");
  this.mPropsBoxObject = this.mPropsTree.treeBoxObject;
}

StyleRulesViewer.prototype = 
{

  ////////////////////////////////////////////////////////////////////////////
  //// Initialization
  
  mRuleDS: null,
  mDecDS: null,
  mSubject: null,

  ////////////////////////////////////////////////////////////////////////////
  //// interface inIViewer

  get uid() { return "styleRules" },
  get pane() { return this.mPane },
  
  get selection() { return null },
  
  get subject() { return this.mSubject },
  set subject(aObject)
  {
    this.mSubject = aObject;
    // update the rule tree
    this.mRuleView = new StyleRuleView(aObject);
    this.mRuleBoxObject.view = this.mRuleView;
    // clear the props tree
    this.mPropsView = null;
    this.mPropsBoxObject.view = null;
    
    this.mObsMan.dispatchEvent("subjectChange", { subject: aObject });
  },

  initialize: function(aPane)
  {
    this.mPane = aPane;
    aPane.notifyViewerReady(this);
  },

  destroy: function()
  {
    // We need to remove the views at this time or else they will attempt to 
    // re-paint while the document is being deconstructed, resulting in
    // some nasty XPConnect assertions
    this.mRuleBoxObject.view = null;
    this.mPropsBoxObject.view = null;
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
  //// UI Commands

  //////// rule contextual commands

  cmdNewRule: function()
  {
  },
  
  cmdToggleSelectedRule: function()
  {
  },

  cmdDeleteSelectedRule: function()
  {
  },

  cmdOpenSelectedFileInEditor: function()
  {
    var item = this.mRuleTree.selectedItems[0];
    if (item)
    {
      var path = null;

      var url = InsUtil.getDSProperty(this.mRuleDS, item.id, "FileURL");
      if (url.substr(0, 6) == "chrome") {
        // This is a tricky situation.  Finding the source of a chrome file means knowing where
        // your build tree is located, and where in the tree the file came from.  Since files
        // from the build tree get copied into chrome:// from unpredictable places, the best way
        // to find them is manually.
        // 
        // Solution to implement: Have the user pick the file location the first time it is opened.
        //  After that, keep the result in some sort of registry to look up next time it is opened.
        //  Allow this registry to be edited via preferences.
      } else if (url.substr(0, 4) == "file") {
        path = url;
      }

      if (path) {
        try {
          var exe = XPCU.createInstance("@mozilla.org/file/local;1", "nsILocalFile");
          exe.initWithPath("c:\\windows\\notepad.exe");
          exe      = exe.nsIFile;
          var C    = Components;
          var proc = C.classes['@mozilla.org/process/util;1'].createInstance
                       (C.interfaces.nsIProcess);
          proc.init(exe);
          proc.run(false, [url], 1);
        } catch (ex) {
          alert("Unable to open editor.");
        }
      }
    }
  },
  
  //////// property contextual commands
  
  cmdNewProperty: function()
  {
    var propname = prompt("Enter the property name:", "");
    if (!propname) return;
    var propval = prompt("Enter the property value:", "");
    if (!propval) return;
    
    var style = this.getSelectedRule().style;
    style.setProperty(propname, propval, "");
    this.mPropsBoxObject.invalidate();
  },
  
  cmdEditSelectedProperty: function()
  {
    var style = this.getSelectedRule().style;
    var propname = this.getSelectedProp();
    var propval = style.getPropertyValue(propname);
    var priority = style.getPropertyPriority(propname);

    propval = prompt("Enter the property value:", propval);
    if (!propval) return;

    style.removeProperty(propname);
    style.setProperty(propname, propval, priority);
    this.mPropsBoxObject.invalidate();
  },

  cmdDeleteSelectedProperty: function()
  {
    var style = this.getSelectedRule().style;
    var propname = this.getSelectedProp();
    style.removeProperty(propname);
    this.mPropsBoxObject.invalidate();
  },

  cmdToggleSelectedImportant: function()
  {
    var style = this.getSelectedRule().style;
    var propname = this.getSelectedProp();
    var propval = style.getPropertyValue(propname);

    var priority = style.getPropertyPriority(propname);
    priority = priority == "important" ? "" : "important";

    style.removeProperty(propname);
    style.setProperty(propname, propval, priority);
    
    this.mPropsBoxObject.invalidate();
  },
  
  ////////////////////////////////////////////////////////////////////////////
  //// Uncategorized

  getSelectedRule: function()
  {
    var idx = this.mRuleTree.currentIndex;
    return this.mRuleView.getRuleAt(idx);
  },

  getSelectedProp: function()
  {
    var rule = this.getSelectedRule();
    var idx = this.mPropsTree.currentIndex;
    return rule.style.item(idx);
  },
  
  onRuleSelect: function()
  {
    var rule = this.getSelectedRule();
    this.mPropsView = new StylePropsView(rule);
    this.mPropsBoxObject.view = this.mPropsView;
  },

  onCreateRulePopup: function()
  {
  }

};

function doesQI(aObject, aInterface)
{
  if (!("QueryInterface" in aObject)) return false;
  
  try {
    var result = aObject.QueryInterface(Components.interfaces[aInterface]);
    return true;
  } catch (ex) {
    return false;
  }
}

////////////////////////////////////////////////////////////////////////////
//// StyleRuleView

function StyleRuleView(aObject)
{
  this.mDOMUtils = XPCU.createInstance("@mozilla.org/inspector/dom-utils;1", "inIDOMUtils");
  if (doesQI(aObject, "nsIDOMCSSStyleSheet")) {
    this.mSheetRules = aObject.cssRules;
  } else {
    this.mRules = this.mDOMUtils.getStyleRules(aObject);
  }

  if (this.mRules) {
    for (var i = this.mRules.Count(); i >= 0; --i) {
      var rule = this.mRules.GetElementAt(i);
      try {
        rule = XPCU.QI(rule, "nsIDOMCSSStyleRule");
      } catch (ex) {
        this.mRules.RemoveElement(rule);
      }
    }
  }
}

StyleRuleView.prototype = new inBaseTreeView();

StyleRuleView.prototype.mSheetRules = null;
StyleRuleView.prototype.mRules = null;

StyleRuleView.prototype.__defineGetter__("rowCount",
function() 
{
  return this.mRules ? this.mRules.Count() : this.mSheetRules ? this.mSheetRules.length : 0;
});

StyleRuleView.prototype.getRuleAt = 
function(aRow) 
{
  if (this.mRules) {
    var rule = this.mRules.GetElementAt(aRow);
    try {
      return XPCU.QI(rule, "nsIDOMCSSStyleRule");
    } catch (ex) {
      return null;
    }
  } else
    return this.mSheetRules[aRow];
}

StyleRuleView.prototype.getCellText = 
function(aRow, aColId) 
{
  if (aRow > this.rowCount) return "";
  
  var rule = this.getRuleAt(aRow);
  if (!rule) return "";
  
  if (aColId == "olcRule") {
    switch (rule.type) {
      case CSSRule.STYLE_RULE:
        return rule.selectorText;
      case CSSRule.IMPORT_RULE:
        // XXX should use .cssText here, but that asserts about some media crap
        return "@import url(" + rule.href + ");";
      default:
        return rule.cssText;
    }
  }

  if (aColId == "olcFileURL") {
    return rule.parentStyleSheet ? rule.parentStyleSheet.href : "";
  }

  if (aColId == "olcWeight") {
    return rule.type == CSSRule.STYLE_RULE ? this.mDOMUtils.getRuleWeight(rule) : "";
  }

  if (aColId == "olcLine") {
    return rule.type == CSSRule.STYLE_RULE ? this.mDOMUtils.getRuleLine(rule) : "";
  }

  return "";
}

////////////////////////////////////////////////////////////////////////////
//// StylePropsView

function StylePropsView(aRule)
{
  this.mDec = aRule.style;
}

StylePropsView.prototype = new inBaseTreeView();

StylePropsView.prototype.__defineGetter__("rowCount",
function() 
{
  return this.mDec ? this.mDec.length : 0;
});

StylePropsView.prototype.getCellProperties = 
function(aRow, aColId, aProperties) 
{
  if (aColId == "olcPropPriority") {
    var prop = this.mDec.item(aRow);
    if (this.mDec.getPropertyPriority(prop) == "important") {
      aProperties.AppendElement(this.createAtom("important"));
    }
  }
}

StylePropsView.prototype.getCellText = 
function(aRow, aColId) 
{
  var prop = this.mDec.item(aRow);
  
  if (aColId == "olcPropName") {
    return prop;
  } else if (aColId == "olcPropValue") {
    return this.mDec.getPropertyValue(prop)
  }
  
  return "";
}

