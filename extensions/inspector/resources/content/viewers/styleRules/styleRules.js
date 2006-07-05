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
 *   Jason Barnabe <jason_barnabe@fastmail.fm>
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

//XXX Don't use anonymous functions
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

  isCommandEnabled: function isCommandEnabled(aCommand)
  {
    switch (aCommand) {
      case "cmdEditCopy":
      case "cmdEditDelete":
      case "cmdTogglePriority":
        return this.mPropsTree.view.selection.count > 0;
      case "cmdEditInsert":
        return this.mRuleTree.view.selection.count == 1;
      case "cmdEditEdit":
        return this.mPropsTree.view.selection.count == 1;
    }
    return false;
  },
  
  getCommand: function getCommand(aCommand)
  {
    switch (aCommand) {
      case "cmdEditCopy":
        return new cmdEditCopy(this.mPropsView.getSelectedRowObjects());
      case "cmdEditDelete":
        return new cmdEditDelete(this.getSelectedDec(),
                                 this.mPropsView.getSelectedRowObjects());
      case "cmdEditInsert":
        var bundle = this.mPanel.panelset.stringBundle;
        var msg = bundle.getString("styleRulePropertyName.message");
        var title = bundle.getString("styleRuleNewProperty.title");

        var property = { value: "" };
        var value = { value: "" };
        var dummy = { value: false };

        if (!gPromptService.prompt(window, title, msg, property, null, dummy)) {
          return null;
        }

        msg = bundle.getString("styleRulePropertyValue.message");
        if (!gPromptService.prompt(window, title, msg, value, null, dummy)) {
          return null;
        }

        return new cmdEditInsert(this.getSelectedDec(), property.value,
                                  value.value, "");
      case "cmdEditEdit":
        var rule = this.getSelectedDec();
        var property = this.getSelectedProp();
        var priority = rule.getPropertyPriority(property);

        var bundle = this.mPanel.panelset.stringBundle;
        var msg = bundle.getString("styleRulePropertyValue.message");
        var title = bundle.getString("styleRuleEditProperty.title");

        var value = { value: rule.getPropertyValue(property) };
        var dummy = { value: false };

        if (!gPromptService.prompt(window, title, msg, value, null, dummy)) {
          return null;
        }

        return new cmdEditEdit(rule, property, value.value, priority);
      case "cmdTogglePriority":
        return new cmdTogglePriority(this.getSelectedDec(),
                                     this.mPropsView.getSelectedRowObjects());
    }
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
    viewer.pane.panelset.updateAllCommands();
  },

  onPropSelect: function()
  {
    viewer.pane.panelset.updateAllCommands();
  },

  onCreateRulePopup: function()
  {
  },

  propOnPopupShowing: function propOnPopupShowing()
  {
    var commandset = document.getElementById("cmdsProps");
    for (var i = 0; i < commandset.childNodes.length; i++) {
      var command = commandset.childNodes[i];
      command.setAttribute("disabled", !viewer.isCommandEnabled(command.id));
    }
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
function (aRow, aCol) 
{
  var prop = this.mDec.item(aRow);
  
  if (aCol.id == "olcPropName") {
    return prop;
  } else if (aCol.id == "olcPropValue") {
    return this.mDec.getPropertyValue(prop)
  }
  
  return null;
}

/**
 * Returns a CSSDeclaration for the row in the tree corresponding to the
 * passed index.
 * @param aIndex index of the row in the tree
 * @return a CSSDeclaration
 */
StylePropsView.prototype.getRowObjectFromIndex = 
function getRowObjectFromIndex(aIndex)
{
  var prop = this.mDec.item(aIndex);
  return new CSSDeclaration(prop, this.mDec.getPropertyValue(prop),
                            this.mDec.getPropertyPriority(prop));
}

/**
 * Handles inserting a CSS declaration
 * @param aRule the rule that will contain the new declaration
 * @param aProperty the property of the new declaration
 * @param aValue the value of the new declaration
 * @param aPriority the priority of the new declaration ("important" or "")
 */
function cmdEditInsert(aRule, aProperty, aValue, aPriority)
{
  this.rule = aRule;
  this.property = aProperty;
  this.value = aValue;
  this.priority = aPriority;
}
cmdEditInsert.prototype =
{
  // remove this line for bug 179621, Phase Three
  txnType: "standard",
  
  // required for nsITransaction
  QueryInterface: txnQueryInterface,
  merge: txnMerge,
  isTransient: false,

  doTransaction: function doTransaction()
  {
    viewer.mPropsBoxObject.beginUpdateBatch();
    try {
      this.rule.setProperty(this.property, this.value, this.priority);
    } finally {
      viewer.mPropsBoxObject.endUpdateBatch();
    }
  },

  undoTransaction: function undoTransaction()
  {
    this.rule.removeProperty(this.property);
    viewer.mPropsBoxObject.invalidate();
  },

  redoTransaction: function redoTransaction()
  {
    this.doTransaction();
  }
}

/**
 * Handles deleting CSS declarations
 * @param aRule the rule containing the declarations
 * @param aDeclarations an array of CSSDeclarations to delete
 */
function cmdEditDelete(aRule, aDeclarations)
{
  this.rule = aRule;
  this.declarations = aDeclarations;
}
cmdEditDelete.prototype =
{
  // remove this line for bug 179621, Phase Three
  txnType: "standard",
  
  // required for nsITransaction
  QueryInterface: txnQueryInterface,
  merge: txnMerge,
  isTransient: false,

  doTransaction: function doTransaction()
  {
    viewer.mPropsBoxObject.beginUpdateBatch();
    for (var i = 0; i < this.declarations.length; i++)
      this.rule.removeProperty(this.declarations[i].property);
    viewer.mPropsBoxObject.endUpdateBatch();
  },

  undoTransaction: function undoTransaction()
  {
    viewer.mPropsBoxObject.beginUpdateBatch();
    for (var i = 0; i < this.declarations.length; i++)
      this.rule.setProperty(this.declarations[i].property,
                            this.declarations[i].value,
                            this.declarations[i].important ? "important" : "");
  },

  redoTransaction: function redoTransaction()
  {
    this.doTransaction();
  }
}

/**
 * Handles editing CSS declarations
 * @param aRule the rule containing the declaration
 * @param aProperty the property to change
 * @param aNewValue the new value for the property
 * @param aNewValue the new priority for the property ("important" or "")
 */
function cmdEditEdit(aRule, aProperty, aNewValue, aNewPriority)
{
  this.rule = aRule;
  this.property = aProperty;
  this.oldValue = aRule.getPropertyValue(aProperty);
  this.newValue = aNewValue;
  this.oldPriority = aRule.getPropertyPriority(aProperty);
  this.newPriority = aNewPriority;
}
cmdEditEdit.prototype =
{
  // remove this line for bug 179621, Phase Three
  txnType: "standard",
  
  // required for nsITransaction
  QueryInterface: txnQueryInterface,
  merge: txnMerge,
  isTransient: false,

  doTransaction: function doTransaction()
  {
    this.rule.setProperty(this.property, this.newValue,
                          this.newPriority);
    viewer.mPropsBoxObject.invalidate();
  },

  undoTransaction: function undoTransaction()
  {
    this.rule.setProperty(this.property, this.oldValue,
                          this.oldPriority);
    viewer.mPropsBoxObject.invalidate();
  },

  redoTransaction: function redoTransaction()
  {
    this.doTransaction();
  }
}

/**
 * Handles toggling CSS !important.
 * @param aRule the rule containing the declarations
 * @param aDeclarations an array of CSSDeclarations to toggle
 */
function cmdTogglePriority(aRule, aDeclarations)
{
  this.rule = aRule;
  this.declarations = aDeclarations;
}
cmdTogglePriority.prototype =
{
  // remove this line for bug 179621, Phase Three
  txnType: "standard",
  
  // required for nsITransaction
  QueryInterface: txnQueryInterface,
  merge: txnMerge,
  isTransient: false,

  doTransaction: function doTransaction()
  {
    for (var i = 0; i < this.declarations.length; i++) {
      //XXX bug 305761 means we can't make something not important, so instead
      // we'll delete this property and make a new one at the proper priority.
      // this method also sucks because the property gets moved to the bottom.
      var property = this.declarations[i].property;
      var value = this.declarations[i].value;
      var newPriority = this.rule.getPropertyPriority(property) == "" ?
                          "important" : "";
      this.rule.removeProperty(property);
      this.rule.setProperty(property, value, newPriority);
    }
    viewer.mPropsBoxObject.invalidate();
  },

  undoTransaction: function undoTransaction()
  {
    this.doTransaction();
  },

  redoTransaction: function redoTransaction()
  {
    this.doTransaction();
  }
}
