/* ***** BEGIN LICENSE BLOCK *****
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
var gPromptService;

//////////// global constants ////////////////////

var kCSSRuleDataSourceIID = "@mozilla.org/rdf/datasource;1?name=Inspector_CSSRules";
var kCSSDecDataSourceIID  = "@mozilla.org/rdf/datasource;1?name=Inspector_CSSDec";
var kPromptServiceCID     = "@mozilla.org/embedcomp/prompt-service;1";

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

  gPromptService = XPCU.getService(kPromptServiceCID, "nsIPromptService");
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
  mPanel: null,

  ////////////////////////////////////////////////////////////////////////////
  //// interface inIViewer

  get uid() { return "styleRules" },
  get pane() { return this.mPanel },
  
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
    this.mPanel = aPane;
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
    var bundle = this.mPanel.panelset.stringBundle;
    var msg = bundle.getString("styleRulePropertyName.message");
    var title = bundle.getString("styleRuleNewProperty.title");

    var propName = { value: "" };
    var propValue = { value: "" };
    var dummy = { value: false };

    if (!gPromptService.prompt(window, title, msg, propName, null, dummy)) {
      return;
    }

    msg = bundle.getString("styleRulePropertyValue.message");
    if (!gPromptService.prompt(window, title, msg, propValue, null, dummy)) {
      return;
    }

    this.mPropsBoxObject.beginUpdateBatch();
    var style = this.getSelectedDec();
    style.setProperty(propName.value, propValue.value, "");
    this.mPropsBoxObject.endUpdateBatch();
  },
  
  cmdEditSelectedProperty: function()
  {
    var style = this.getSelectedDec();
    var propname = this.getSelectedProp();
    var propval = style.getPropertyValue(propname);
    var priority = style.getPropertyPriority(propname);

    var bundle = this.mPanel.panelset.stringBundle;
    var msg = bundle.getString("styleRulePropertyValue.message");
    var title = bundle.getString("styleRuleEditProperty.title");

    var propValue = { value: propval };
    var dummy = { value: false };

    if (!gPromptService.prompt(window, title, msg, propValue, null, dummy)) {
      return;
    }

    style.removeProperty(propname);
    style.setProperty(propname, propValue.value, priority);
    this.mPropsBoxObject.invalidate();
  },

  cmdDeleteSelectedProperty: function()
  {
    this.mPropsBoxObject.beginUpdateBatch();
    var style = this.getSelectedDec();
    var propname = this.getSelectedProp();
    style.removeProperty(propname);
    this.mPropsBoxObject.endUpdateBatch();
  },

  cmdToggleSelectedImportant: function()
  {
    var style = this.getSelectedDec();
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

  getSelectedDec: function()
  {
    var idx = this.mRuleTree.currentIndex;
    return this.mRuleView.getDecAt(idx);
  },

  getSelectedProp: function()
  {
    var dec = this.getSelectedDec();
    var idx = this.mPropsTree.currentIndex;
    return dec.item(idx);
  },
  
  onRuleSelect: function()
  {
    var dec = this.getSelectedDec();
    this.mPropsView = new StylePropsView(dec);
    this.mPropsBoxObject.view = this.mPropsView;
  },

  onCreateRulePopup: function()
  {
  }

};

////////////////////////////////////////////////////////////////////////////
//// StyleRuleView

function StyleRuleView(aObject)
{
  this.mDOMUtils = XPCU.getService("@mozilla.org/inspector/dom-utils;1", "inIDOMUtils");
  if (aObject instanceof Components.interfaces.nsIDOMCSSStyleSheet) {
    this.mSheetRules = aObject.cssRules;
  } else {
    this.mRules = this.mDOMUtils.getCSSStyleRules(aObject);
    if (aObject.hasAttribute("style")) {
      try {
        this.mStyleAttribute =
          new XPCNativeWrapper(aObject, "style").style;
      } catch (ex) {}
    }
  }
}

StyleRuleView.prototype = new inBaseTreeView();

StyleRuleView.prototype.mSheetRules = null;
StyleRuleView.prototype.mRules = null;
StyleRuleView.prototype.mStyleAttribute = null;

StyleRuleView.prototype.__defineGetter__("rowCount",
function() 
{
  return this.mRules ? this.mRules.Count() + (this.mStyleAttribute ? 1 : 0)
                     : this.mSheetRules ? this.mSheetRules.length : 0;
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
  }
  return this.mSheetRules[aRow];
}

StyleRuleView.prototype.getDecAt = 
function(aRow) 
{
  if (this.mRules) {
    if (this.mStyleAttribute && aRow + 1 == this.rowCount) {
      return this.mStyleAttribute;
    }
    var rule = this.mRules.GetElementAt(aRow);
    try {
      return XPCU.QI(rule, "nsIDOMCSSStyleRule").style;
    } catch (ex) {
      return null;
    }
  }
  return this.mSheetRules[aRow].style;
}

StyleRuleView.prototype.getCellText = 
function(aRow, aCol) 
{
  if (aRow > this.rowCount) return "";

  // special case for the style attribute
  if (this.mStyleAttribute && aRow + 1 == this.rowCount) {
    if (aCol.id == "olcRule") {
      return 'style=""';
    }

    if (aCol.id == "olcFileURL") {
      // we ought to be able to get to the URL...
      return "";
    }

    if (aCol.id == "olcLine") {
      return "";
    }
    return "";
  }
  
  var rule = this.getRuleAt(aRow);
  if (!rule) return "";
  
  if (aCol.id == "olcRule") {
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

  if (aCol.id == "olcFileURL") {
    return rule.parentStyleSheet ? rule.parentStyleSheet.href : "";
  }

  if (aCol.id == "olcLine") {
    return rule.type == CSSRule.STYLE_RULE ? this.mDOMUtils.getRuleLine(rule) : "";
  }

  return "";
}

////////////////////////////////////////////////////////////////////////////
//// StylePropsView

function StylePropsView(aDec)
{
  this.mDec = aDec;
}

StylePropsView.prototype = new inBaseTreeView();

StylePropsView.prototype.__defineGetter__("rowCount",
function() 
{
  return this.mDec ? this.mDec.length : 0;
});

StylePropsView.prototype.getCellProperties = 
function(aRow, aCol, aProperties) 
{
  if (aCol.id == "olcPropPriority") {
    var prop = this.mDec.item(aRow);
    if (this.mDec.getPropertyPriority(prop) == "important") {
      aProperties.AppendElement(this.createAtom("important"));
    }
  }
}

StylePropsView.prototype.getCellText = 
function(aRow, aCol) 
{
  var prop = this.mDec.item(aRow);
  
  if (aCol.id == "olcPropName") {
    return prop;
  } else if (aCol.id == "olcPropValue") {
    return this.mDec.getPropertyValue(prop)
  }
  
  return "";
}

