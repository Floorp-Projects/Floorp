/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
 * The Original Code is the Mozilla Inspector Module.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Walker (jwalker@mozilla.com) (original author)
 *   Mihai Șucan <mihai.sucan@gmail.com>
 *   Michael Ratcliffe <mratcliffe@mozilla.com>
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

const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PluralForm.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/devtools/CssLogic.jsm");
Cu.import("resource:///modules/devtools/Templater.jsm");

var EXPORTED_SYMBOLS = ["CssHtmlTree", "PropertyView"];

/**
 * CssHtmlTree is a panel that manages the display of a table sorted by style.
 * There should be one instance of CssHtmlTree per style display (of which there
 * will generally only be one).
 *
 * @params {Document} aStyleWin The main XUL browser document
 * @params {CssLogic} aCssLogic How we dig into the CSS. See CssLogic.jsm
 * @constructor
 */
function CssHtmlTree(aStyleWin, aCssLogic, aPanel)
{
  this.styleWin = aStyleWin;
  this.cssLogic = aCssLogic;
  this.doc = aPanel.ownerDocument;
  this.win = this.doc.defaultView;
  this.getRTLAttr = CssHtmlTree.getRTLAttr;
  this.propertyViews = {};

  // The document in which we display the results (csshtmltree.xhtml).
  this.styleDocument = this.styleWin.contentWindow.document;

  // Nodes used in templating
  this.root = this.styleDocument.getElementById("root");
  this.templateRoot = this.styleDocument.getElementById("templateRoot");
  this.propertyContainer = this.styleDocument.getElementById("propertyContainer");
  this.templateProperty = this.styleDocument.getElementById("templateProperty");
  this.panel = aPanel;

  // The element that we're inspecting, and the document that it comes from.
  this.viewedElement = null;
  this.createStyleViews();
}

/**
 * Memonized lookup of a l10n string from a string bundle.
 * @param {string} aName The key to lookup.
 * @returns A localized version of the given key.
 */
CssHtmlTree.l10n = function CssHtmlTree_l10n(aName)
{
  try {
    return CssHtmlTree._strings.GetStringFromName(aName);
  } catch (ex) {
    Services.console.logStringMessage("Error reading '" + aName + "'");
    throw new Error("l10n error with " + aName);
  }
};

/**
 * Clone the given template node, and process it by resolving ${} references
 * in the template.
 *
 * @param {nsIDOMElement} aTemplate the template note to use.
 * @param {nsIDOMElement} aDestination the destination node where the
 * processed nodes will be displayed.
 * @param {object} aData the data to pass to the template.
 * @param {Boolean} aPreserveDestination If true then the template will be
 * appended to aDestination's content else aDestination.innerHTML will be
 * cleared before the template is appended.
 */
CssHtmlTree.processTemplate = function CssHtmlTree_processTemplate(aTemplate,
                                  aDestination, aData, aPreserveDestination)
{
  if (!aPreserveDestination) {
    aDestination.innerHTML = "";
  }

  // All the templater does is to populate a given DOM tree with the given
  // values, so we need to clone the template first.
  let duplicated = aTemplate.cloneNode(true);
  new Templater().processNode(duplicated, aData);
  while (duplicated.firstChild) {
    aDestination.appendChild(duplicated.firstChild);
  }
};

/**
 * Checks whether the UI is RTL
 * @return {Boolean} true or false
 */
CssHtmlTree.isRTL = function CssHtmlTree_isRTL()
{
  return CssHtmlTree.getRTLAttr == "rtl";
};

/**
 * Checks whether the UI is RTL
 * @return {String} "ltr" or "rtl"
 */
XPCOMUtils.defineLazyGetter(CssHtmlTree, "getRTLAttr", function() {
  let mainWindow = Services.wm.getMostRecentWindow("navigator:browser");
  return mainWindow.getComputedStyle(mainWindow.gBrowser).direction;
});

XPCOMUtils.defineLazyGetter(CssHtmlTree, "_strings", function() Services.strings
    .createBundle("chrome://browser/locale/styleinspector.properties"));

CssHtmlTree.prototype = {
  htmlComplete: false,

  /**
   * Update the highlighted element. The CssHtmlTree panel will show the style
   * information for the given element.
   * @param {nsIDOMElement} aElement The highlighted node to get styles for.
   */
  highlight: function CssHtmlTree_highlight(aElement)
  {
    if (this.viewedElement == aElement) {
      return;
    }

    this.viewedElement = aElement;

    CssHtmlTree.processTemplate(this.templateRoot, this.root, this);

    if (this.htmlComplete) {
      this.refreshPanel();
    } else {
      // We use a setTimeout loop to display the properties in batches of 15 at a
      // time. This results in a perceptibly more responsive UI.
      let i = 0;
      let batchSize = 15;
      let max = CssHtmlTree.propertyNames.length - 1;
      function displayProperties() {
        if (this.viewedElement == aElement && this.panel.isOpen()) {
          // Display the next 15 properties
          for (let step = i + batchSize; i < step && i <= max; i++) {
            let name = CssHtmlTree.propertyNames[i];
            let propView = new PropertyView(this, name);
            CssHtmlTree.processTemplate(this.templateProperty,
              this.propertyContainer, propView, true);
            propView.refreshMatchedSelectors();
            propView.refreshUnmatchedSelectors();
            this.propertyViews[name] = propView;
          }
          if (i < max) {
            // There are still some properties to display. We loop here to display
            // the next batch of 15.
            this.win.setTimeout(displayProperties.bind(this), 50);
          } else {
            this.htmlComplete = true;
          }
        }
      }
      this.win.setTimeout(displayProperties.bind(this), 50);
    }
  },

  /**
   * Refresh the panel content.
   */
  refreshPanel: function CssHtmlTree_refreshPanel()
  {
    for each(let propView in this.propertyViews) {
      propView.refresh();
    }
  },

  /**
   * Called when the user clicks on a parent element in the "current element"
   * path.
   *
   * @param {Event} aEvent the DOM Event object.
   */
  pathClick: function CssHtmlTree_pathClick(aEvent)
  {
    aEvent.preventDefault();
    if (aEvent.target && this.viewedElement != aEvent.target.pathElement) {
      if (this.win.InspectorUI.selection) {
        if (aEvent.target.pathElement != this.win.InspectorUI.selection) {
          let elt = aEvent.target.pathElement;
          this.win.InspectorUI.inspectNode(elt);
          this.panel.selectNode(elt);
        }
      } else {
        this.panel.selectNode(aEvent.target.pathElement);
      }
    }
  },

  /**
   * Provide access to the path to get from document.body to the selected
   * element.
   *
   * @return {array} the array holding the path from document.body to the
   * selected element.
   */
  get pathElements()
  {
    return CssLogic.getShortNamePath(this.viewedElement);
  },

  /**
   * The CSS as displayed by the UI.
   */
  createStyleViews: function CssHtmlTree_createStyleViews()
  {
    if (CssHtmlTree.propertyNames) {
      return;
    }

    CssHtmlTree.propertyNames = [];

    // Here we build and cache a list of css properties supported by the browser
    // We could use any element but let's use the main document's body
    let styles = this.styleWin.contentWindow.getComputedStyle(this.styleDocument.body);
    let mozProps = [];
    for (let i = 0, numStyles = styles.length; i < numStyles; i++) {
      let prop = styles.item(i);
      if (prop.charAt(0) == "-") {
        mozProps.push(prop);
      } else {
        CssHtmlTree.propertyNames.push(prop);
      }
    }

    CssHtmlTree.propertyNames.sort();
    CssHtmlTree.propertyNames.push.apply(CssHtmlTree.propertyNames,
      mozProps.sort());
  },
};

/**
 * A container to give easy access to property data from the template engine.
 *
 * @constructor
 * @param {CssHtmlTree} aTree the CssHtmlTree instance we are working with.
 * @param {string} aName the CSS property name for which this PropertyView
 * instance will render the rules.
 */
function PropertyView(aTree, aName)
{
  this.tree = aTree;
  this.name = aName;
  this.getRTLAttr = CssHtmlTree.getRTLAttr;

  this.link = "https://developer.mozilla.org/en/CSS/" + aName;

  this.templateMatchedSelectors = aTree.styleDocument.getElementById("templateMatchedSelectors");
  this.templateUnmatchedSelectors = aTree.styleDocument.getElementById("templateUnmatchedSelectors");
}

PropertyView.prototype = {
  // The parent element which contains the open attribute
  element: null,

  // Destination for property values
  valueNode: null,

  // Are matched rules expanded?
  matchedExpanded: false,

  // Are unmatched rules expanded?
  unmatchedExpanded: false,

  // Matched selector container
  matchedSelectorsContainer: null,

  // Unmatched selector container
  unmatchedSelectorsContainer: null,

  // Matched selector expando
  matchedExpander: null,

  // Unmatched selector expando
  unmatchedExpander: null,

  // Container for X matched selectors
  matchedSelectorsTitleNode: null,

  // Container for X unmatched selectors
  unmatchedSelectorsTitleNode: null,

  // Matched selectors table
  matchedSelectorTable: null,

  // Unmatched selectors table
  unmatchedSelectorTable: null,

  // Cache for matched selector views
  _matchedSelectorViews: null,

  // Cache for unmatched selector views
  _unmatchedSelectorViews: null,

  // The previously selected element used for the selector view caches
  prevViewedElement: null,

  /**
   * Get the computed style for the current property.
   *
   * @return {string} the computed style for the current property of the
   * currently highlighted element.
   */
  get value()
  {
    return this.propertyInfo.value;
  },

  /**
   * An easy way to access the CssPropertyInfo behind this PropertyView.
   */
  get propertyInfo()
  {
    return this.tree.cssLogic.getPropertyInfo(this.name);
  },

  /**
   * The number of matched selectors.
   */
  get matchedSelectorCount()
  {
    return this.propertyInfo.matchedSelectors.length;
  },

  /**
   * The number of unmatched selectors.
   */
  get unmatchedSelectorCount()
  {
    return this.propertyInfo.unmatchedSelectors.length;
  },

  /**
   * Refresh the panel's CSS property value.
   */
  refresh: function PropertyView_refresh()
  {
    if (!this.tree.viewedElement) {
      this.valueNode.innerHTML = "";
      this.matchedSelectorsContainer.hidden = true;
      this.unmatchedSelectorsContainer.hidden = true;
      this.matchedSelectorTable.innerHTML = "";
      this.unmatchedSelectorTable.innerHTML = "";
      this.matchedExpander.removeAttribute("open");
      this.unmatchedExpander.removeAttribute("open");
      return;
    }

    this.valueNode.innerHTML = this.propertyInfo.value;

    if (this.prevViewedElement != this.tree.viewedElement) {
      this._matchedSelectorViews = null;
      this._unmatchedSelectorViews = null;
      this.prevViewedElement = this.tree.viewedElement;
    }

    this.refreshMatchedSelectors();
    this.refreshUnmatchedSelectors();
  },

  /**
   * Refresh the panel matched rules.
   */
  refreshMatchedSelectors: function PropertyView_refreshMatchedSelectors()
  {
    this.matchedSelectorsTitleNode.innerHTML = this.matchedSelectorTitle();
    this.matchedSelectorsContainer.hidden = this.matchedSelectorCount == 0;

    if (this.matchedExpanded && this.matchedSelectorCount > 0) {
      CssHtmlTree.processTemplate(this.templateMatchedSelectors,
        this.matchedSelectorTable, this);
      this.matchedExpander.setAttribute("open", "");
    } else {
      this.matchedSelectorTable.innerHTML = "";
      this.matchedExpander.removeAttribute("open");
    }
  },

  /**
   * Refresh the panel unmatched rules.
   */
  refreshUnmatchedSelectors: function PropertyView_refreshUnmatchedSelectors() {
    this.unmatchedSelectorsTitleNode.innerHTML = this.unmatchedSelectorTitle();
    this.unmatchedSelectorsContainer.hidden = this.unmatchedSelectorCount == 0;

    if (this.unmatchedExpanded && this.unmatchedSelectorCount > 0) {
      CssHtmlTree.processTemplate(this.templateUnmatchedSelectors,
          this.unmatchedSelectorTable, this);
      this.unmatchedExpander.setAttribute("open", "");
    } else {
      this.unmatchedSelectorTable.innerHTML = "";
      this.unmatchedExpander.removeAttribute("open");
    }
  },

  /**
   * Compute the title of the matched selector expander. The title includes the
   * number of selectors that match the currently selected element.
   *
   * @return {string} The rule title.
   */
  matchedSelectorTitle: function PropertyView_matchedSelectorTitle()
  {
    let result = "";

    if (this.matchedSelectorCount > 0) {
      let str = CssHtmlTree.l10n("property.numberOfMatchedSelectors");
      result = PluralForm.get(this.matchedSelectorCount, str)
                         .replace("#1", this.matchedSelectorCount);
    }
    return result;
  },

  /**
   * Compute the title of the unmatched selector expander. The title includes
   * the number of selectors that match the currently selected element.
   *
   * @return {string} The rule title.
   */
  unmatchedSelectorTitle: function PropertyView_unmatchedSelectorTitle()
  {
    let result = "";

    if (this.unmatchedSelectorCount > 0) {
      let str = CssHtmlTree.l10n("property.numberOfUnmatchedSelectors");
      result = PluralForm.get(this.unmatchedSelectorCount, str)
                         .replace("#1", this.unmatchedSelectorCount);
    }
    return result;
  },

  /**
   * Provide access to the matched SelectorViews that we are currently
   * displaying.
   */
  get matchedSelectorViews()
  {
    if (!this._matchedSelectorViews) {
      this._matchedSelectorViews = [];
      this.propertyInfo.matchedSelectors.forEach(
        function matchedSelectorViews_convert(aSelectorInfo) {
          this._matchedSelectorViews.push(new SelectorView(aSelectorInfo));
        }, this);
    }

    return this._matchedSelectorViews;
  },

    /**
   * Provide access to the unmatched SelectorViews that we are currently
   * displaying.
   */
  get unmatchedSelectorViews()
  {
    if (!this._unmatchedSelectorViews) {
      this._unmatchedSelectorViews = [];
      this.propertyInfo.unmatchedSelectors.forEach(
        function unmatchedSelectorViews_convert(aSelectorInfo) {
          this._unmatchedSelectorViews.push(new SelectorView(aSelectorInfo));
        }, this);
    }

    return this._unmatchedSelectorViews;
  },

  /**
   * The action when a user expands matched selectors.
   */
  matchedSelectorsClick: function PropertyView_matchedSelectorsClick(aEvent)
  {
    this.matchedExpanded = !this.matchedExpanded;
    this.refreshMatchedSelectors();
    aEvent.preventDefault();
  },

  /**
   * The action when a user expands unmatched selectors.
   */
  unmatchedSelectorsClick: function PropertyView_unmatchedSelectorsClick(aEvent)
  {
    this.unmatchedExpanded = !this.unmatchedExpanded;
    this.refreshUnmatchedSelectors();
    aEvent.preventDefault();
  },
};

/**
 * A container to view us easy access to display data from a CssRule
 */
function SelectorView(aSelectorInfo)
{
  this.selectorInfo = aSelectorInfo;
  this._cacheStatusNames();
}

/**
 * Decode for cssInfo.rule.status
 * @see SelectorView.prototype._cacheStatusNames
 * @see CssLogic.STATUS
 */
SelectorView.STATUS_NAMES = [
  // "Unmatched", "Parent Match", "Matched", "Best Match"
];

SelectorView.CLASS_NAMES = [
  "unmatched", "parentmatch", "matched", "bestmatch"
];

SelectorView.prototype = {
  /**
   * Cache localized status names.
   *
   * These statuses are localized inside the styleinspector.properties string
   * bundle.
   * @see CssLogic.jsm - the CssLogic.STATUS array.
   *
   * @return {void}
   */
  _cacheStatusNames: function SelectorView_cacheStatusNames()
  {
    if (SelectorView.STATUS_NAMES.length) {
      return;
    }

    for (let status in CssLogic.STATUS) {
      let i = CssLogic.STATUS[status];
      if (i > -1) {
        let value = CssHtmlTree.l10n("rule.status." + status);
        // Replace normal spaces with non-breaking spaces
        SelectorView.STATUS_NAMES[i] = value.replace(/ /g, '\u00A0');
      }
    }
  },

  /**
   * A localized version of cssRule.status
   */
  get statusText()
  {
    return SelectorView.STATUS_NAMES[this.selectorInfo.status];
  },

  /**
   * Get class name for selector depending on status
   */
  get statusClass()
  {
    return SelectorView.CLASS_NAMES[this.selectorInfo.status];
  },

  /**
   * A localized Get localized human readable info
   */
  humanReadableText: function SelectorView_humanReadableText(aElement)
  {
    if (CssHtmlTree.isRTL()) {
      return this.selectorInfo.value + " \u2190 " + this.text(aElement);
    } else {
      return this.text(aElement) + " \u2192 " + this.selectorInfo.value;
    }
  },

  text: function SelectorView_text(aElement) {
    let result = this.selectorInfo.selector.text;
    if (this.selectorInfo.elementStyle) {
      if (this.selectorInfo.sourceElement == this.win.InspectorUI.selection) {
        result = "this";
      } else {
        result = CssLogic.getShortName(this.selectorInfo.sourceElement);
        aElement.parentNode.querySelector(".rule-link > a").
          addEventListener("click", function(aEvent) {
            this.win.InspectorUI.inspectNode(this.selectorInfo.sourceElement);
            aEvent.preventDefault();
          }, false);
      }

      result += ".style";
    }
    return result;
  },
};
