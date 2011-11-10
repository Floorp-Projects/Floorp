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
 *   Joe Walker <jwalker@mozilla.com> (original author)
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

/*
 * About the objects defined in this file:
 * - CssLogic contains style information about a view context. It provides
 *   access to 2 sets of objects: Css[Sheet|Rule|Selector] provide access to
 *   information that does not change when the selected element changes while
 *   Css[Property|Selector]Info provide information that is dependent on the
 *   selected element.
 *   Its key methods are highlight(), getPropertyInfo() and forEachSheet(), etc
 *   It also contains a number of static methods for l10n, naming, etc
 *
 * - CssSheet provides a more useful API to a DOM CSSSheet for our purposes,
 *   including shortSource and href.
 * - CssRule a more useful API to a nsIDOMCSSRule including access to the group
 *   of CssSelectors that the rule provides properties for
 * - CssSelector A single selector - i.e. not a selector group. In other words
 *   a CssSelector does not contain ','. This terminology is different from the
 *   standard DOM API, but more inline with the definition in the spec.
 *
 * - CssPropertyInfo contains style information for a single property for the
 *   highlighted element. It divides the CSS rules on the page into matched and
 *   unmatched rules.
 * - CssSelectorInfo is a wrapper around CssSelector, which adds sorting with
 *   reference to the selected element.
 */

/**
 * Provide access to the style information in a page.
 * CssLogic uses the standard DOM API, and the Gecko inIDOMUtils API to access
 * styling information in the page, and present this to the user in a way that
 * helps them understand:
 * - why their expectations may not have been fulfilled
 * - how browsers process CSS
 * @constructor
 */
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

var EXPORTED_SYMBOLS = ["CssLogic"];

function CssLogic()
{
  // The cache of examined CSS properties.
  _propertyInfos: {};
}

/**
 * Special values for filter, in addition to an href these values can be used
 */
CssLogic.FILTER = {
  ALL: "all", // show properties from all user style sheets.
  UA: "ua",   // ALL, plus user-agent (i.e. browser) style sheets
};

/**
 * Known media values. To distinguish "all" stylesheets (above) from "all" media
 * The full list includes braille, embossed, handheld, print, projection,
 * speech, tty, and tv, but this is only a hack because these are not defined
 * in the DOM at all.
 * @see http://www.w3.org/TR/CSS21/media.html#media-types
 */
CssLogic.MEDIA = {
  ALL: "all",
  SCREEN: "screen",
};

/**
 * Each rule has a status, the bigger the number, the better placed it is to
 * provide styling information.
 *
 * These statuses are localized inside the styleinspector.properties string bundle.
 * @see csshtmltree.js RuleView._cacheStatusNames()
 */
CssLogic.STATUS = {
  BEST: 3,
  MATCHED: 2,
  PARENT_MATCH: 1,
  UNMATCHED: 0,
  UNKNOWN: -1,
};

CssLogic.prototype = {
  // Both setup by highlight().
  viewedElement: null,
  viewedDocument: null,

  // The cache of the known sheets.
  _sheets: null,

  // Have the sheets been cached?
  _sheetsCached: false,

  // The total number of rules, in all stylesheets, after filtering.
  _ruleCount: 0,

  // The computed styles for the viewedElement.
  _computedStyle: null,

  // Source filter. Only display properties coming from the given source
  _sourceFilter: CssLogic.FILTER.ALL,

  // Used for tracking unique CssSheet/CssRule/CssSelector objects, in a run of
  // processMatchedSelectors().
  _passId: 0,

  // Used for tracking matched CssSelector objects, such that we can skip them
  // in processUnmatchedSelectors().
  _matchId: 0,

  _matchedRules: null,
  _matchedSelectors: null,
  _unmatchedSelectors: null,

  domUtils: Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils),

  /**
   * Reset various properties
   */
  reset: function CssLogic_reset()
  {
    this._propertyInfos = {};
    this._ruleCount = 0;
    this._sheetIndex = 0;
    this._sheets = {};
    this._sheetsCached = false;
    this._matchedRules = null;
    this._matchedSelectors = null;
    this._unmatchedSelectors = null;
  },

  /**
   * Focus on a new element - remove the style caches.
   *
   * @param {nsIDOMElement} aViewedElement the element the user has highlighted
   * in the Inspector.
   */
  highlight: function CssLogic_highlight(aViewedElement)
  {
    if (!aViewedElement) {
      this.viewedElement = null;
      this.viewedDocument = null;
      this._computedStyle = null;
      this.reset();
      return;
    }

    this.viewedElement = aViewedElement;

    let doc = this.viewedElement.ownerDocument;
    if (doc != this.viewedDocument) {
      // New document: clear/rebuild the cache.
      this.viewedDocument = doc;

      // Hunt down top level stylesheets, and cache them.
      this._cacheSheets();
    } else {
      // Clear cached data in the CssPropertyInfo objects.
      this._propertyInfos = {};
    }

    this._matchedRules = null;
    this._matchedSelectors = null;
    this._unmatchedSelectors = null;
    let win = this.viewedDocument.defaultView;
    this._computedStyle = win.getComputedStyle(this.viewedElement, "");
  },

  /**
   * Get the source filter.
   * @returns {string} The source filter being used.
   */
  get sourceFilter() {
    return this._sourceFilter;
  },

  /**
   * Source filter. Only display properties coming from the given source (web
   * address). Note that in order to avoid information overload we DO NOT show
   * unmatched system rules.
   * @see CssLogic.FILTER.*
   */
  set sourceFilter(aValue) {
    let oldValue = this._sourceFilter;
    this._sourceFilter = aValue;

    let ruleCount = 0;

    // Update the CssSheet objects.
    this.forEachSheet(function(aSheet) {
      aSheet._sheetAllowed = -1;
      if (!aSheet.systemSheet && aSheet.sheetAllowed) {
        ruleCount += aSheet.ruleCount;
      }
    }, this);

    this._ruleCount = ruleCount;

    // Full update is needed because the this.processMatchedSelectors() method
    // skips UA stylesheets if the filter does not allow such sheets.
    let needFullUpdate = (oldValue == CssLogic.FILTER.UA ||
        aValue == CssLogic.FILTER.UA);

    if (needFullUpdate) {
      this._matchedRules = null;
      this._matchedSelectors = null;
      this._unmatchedSelectors = null;
      this._propertyInfos = {};
    } else {
      // Update the CssPropertyInfo objects.
      for each (let propertyInfo in this._propertyInfos) {
        propertyInfo.needRefilter = true;
      }
    }
  },

  /**
   * Return a CssPropertyInfo data structure for the currently viewed element
   * and the specified CSS property. If there is no currently viewed element we
   * return an empty object.
   *
   * @param {string} aProperty The CSS property to look for.
   * @return {CssPropertyInfo} a CssPropertyInfo structure for the given
   * property.
   */
  getPropertyInfo: function CssLogic_getPropertyInfo(aProperty)
  {
    if (!this.viewedElement) {
      return {};
    }

    let info = this._propertyInfos[aProperty];
    if (!info) {
      info = new CssPropertyInfo(this, aProperty);
      this._propertyInfos[aProperty] = info;
    }

    return info;
  },

  /**
   * Cache all the stylesheets in the inspected document
   * @private
   */
  _cacheSheets: function CssLogic_cacheSheets()
  {
    this._passId++;
    this.reset();

    // styleSheets isn't an array, but forEach can work on it anyway
    Array.prototype.forEach.call(this.viewedDocument.styleSheets,
        this._cacheSheet, this);

    this._sheetsCached = true;
  },

  /**
   * Cache a stylesheet if it falls within the requirements: if it's enabled,
   * and if the @media is allowed. This method also walks through the stylesheet
   * cssRules to find @imported rules, to cache the stylesheets of those rules
   * as well.
   *
   * @private
   * @param {CSSStyleSheet} aDomSheet the CSSStyleSheet object to cache.
   */
  _cacheSheet: function CssLogic_cacheSheet(aDomSheet)
  {
    if (aDomSheet.disabled) {
      return;
    }

    // Only work with stylesheets that have their media allowed.
    if (!this.mediaMatches(aDomSheet)) {
      return;
    }

    // Cache the sheet.
    let cssSheet = this.getSheet(aDomSheet, this._sheetIndex++);
    if (cssSheet._passId != this._passId) {
      cssSheet._passId = this._passId;

      // Find import rules.
      Array.prototype.forEach.call(aDomSheet.cssRules, function(aDomRule) {
        if (aDomRule.type == Ci.nsIDOMCSSRule.IMPORT_RULE && aDomRule.styleSheet &&
            this.mediaMatches(aDomRule)) {
          this._cacheSheet(aDomRule.styleSheet);
        }
      }, this);
    }
  },

  /**
   * Retrieve the list of stylesheets in the document.
   *
   * @return {array} the list of stylesheets in the document.
   */
  get sheets()
  {
    if (!this._sheetsCached) {
      this._cacheSheets();
    }

    let sheets = [];
    this.forEachSheet(function (aSheet) {
      if (!aSheet.systemSheet) {
        sheets.push(aSheet);
      }
    }, this);

    return sheets;
  },

  /**
   * Retrieve a CssSheet object for a given a CSSStyleSheet object. If the
   * stylesheet is already cached, you get the existing CssSheet object,
   * otherwise the new CSSStyleSheet object is cached.
   *
   * @param {CSSStyleSheet} aDomSheet the CSSStyleSheet object you want.
   * @param {number} aIndex the index, within the document, of the stylesheet.
   *
   * @return {CssSheet} the CssSheet object for the given CSSStyleSheet object.
   */
  getSheet: function CL_getSheet(aDomSheet, aIndex)
  {
    let cacheId = "";

    if (aDomSheet.href) {
      cacheId = aDomSheet.href;
    } else if (aDomSheet.ownerNode && aDomSheet.ownerNode.ownerDocument) {
      cacheId = aDomSheet.ownerNode.ownerDocument.location;
    }

    let sheet = null;
    let sheetFound = false;

    if (cacheId in this._sheets) {
      for (let i = 0, numSheets = this._sheets[cacheId].length; i < numSheets; i++) {
        sheet = this._sheets[cacheId][i];
        if (sheet.domSheet === aDomSheet) {
          if (aIndex != -1) {
            sheet.index = aIndex;
          }
          sheetFound = true;
          break;
        }
      }
    }

    if (!sheetFound) {
      if (!(cacheId in this._sheets)) {
        this._sheets[cacheId] = [];
      }

      sheet = new CssSheet(this, aDomSheet, aIndex);
      if (sheet.sheetAllowed && !sheet.systemSheet) {
        this._ruleCount += sheet.ruleCount;
      }

      this._sheets[cacheId].push(sheet);
    }

    return sheet;
  },

  /**
   * Process each cached stylesheet in the document using your callback.
   *
   * @param {function} aCallback the function you want executed for each of the
   * CssSheet objects cached.
   * @param {object} aScope the scope you want for the callback function. aScope
   * will be the this object when aCallback executes.
   */
  forEachSheet: function CssLogic_forEachSheet(aCallback, aScope)
  {
    for each (let sheet in this._sheets) {
      sheet.forEach(aCallback, aScope);
    }
  },

  /**
   * Process *some* cached stylesheets in the document using your callback. The
   * callback function should return true in order to halt processing.
   *
   * @param {function} aCallback the function you want executed for some of the
   * CssSheet objects cached.
   * @param {object} aScope the scope you want for the callback function. aScope
   * will be the this object when aCallback executes.
   * @return {Boolean} true if aCallback returns true during any iteration,
   * otherwise false is returned.
   */
  forSomeSheets: function CssLogic_forSomeSheets(aCallback, aScope)
  {
    for each (let sheets in this._sheets) {
      if (sheets.some(aCallback, aScope)) {
        return true;
      }
    }
    return false;
  },

  /**
   * Get the number nsIDOMCSSRule objects in the document, counted from all of
   * the stylesheets. System sheets are excluded. If a filter is active, this
   * tells only the number of nsIDOMCSSRule objects inside the selected
   * CSSStyleSheet.
   *
   * WARNING: This only provides an estimate of the rule count, and the results
   * could change at a later date. Todo remove this
   *
   * @return {number} the number of nsIDOMCSSRule (all rules).
   */
  get ruleCount()
  {
    if (!this._sheetsCached) {
      this._cacheSheets();
    }

    return this._ruleCount;
  },

  /**
   * Process the CssSelector objects that match the highlighted element and its
   * parent elements. aScope.aCallback() is executed for each CssSelector
   * object, being passed the CssSelector object and the match status.
   *
   * This method also includes all of the element.style properties, for each
   * highlighted element parent and for the highlighted element itself.
   *
   * Note that the matched selectors are cached, such that next time your
   * callback is invoked for the cached list of CssSelector objects.
   *
   * @param {function} aCallback the function you want to execute for each of
   * the matched selectors.
   * @param {object} aScope the scope you want for the callback function. aScope
   * will be the this object when aCallback executes.
   */
  processMatchedSelectors: function CL_processMatchedSelectors(aCallback, aScope)
  {
    if (this._matchedSelectors) {
      if (aCallback) {
        this._passId++;
        this._matchedSelectors.forEach(function(aValue) {
          aCallback.call(aScope, aValue[0], aValue[1]);
          aValue[0]._cssRule._passId = this._passId;
        }, this);
      }
      return;
    }

    if (!this._matchedRules) {
      this._buildMatchedRules();
    }

    this._matchedSelectors = [];
    this._unmatchedSelectors = null;
    this._passId++;

    for (let i = 0; i < this._matchedRules.length; i++) {
      let rule = this._matchedRules[i][0];
      let status = this._matchedRules[i][1];

      rule.selectors.forEach(function (aSelector) {
        if (aSelector._matchId !== this._matchId &&
            (aSelector.elementStyle ||
             this._selectorMatchesElement(aSelector))) {
          aSelector._matchId = this._matchId;
          this._matchedSelectors.push([ aSelector, status ]);
          if (aCallback) {
            aCallback.call(aScope, aSelector, status);
          }
        }
      }, this);

      rule._passId = this._passId;
    }
  },

  /**
   * Check if the given selector matches the highlighted element or any of its
   * parents.
   *
   * @private
   * @param {string} aSelector the selector string you want to check.
   * @return {boolean} true if the given selector matches the highlighted
   * element or any of its parents, otherwise false is returned.
   */
  _selectorMatchesElement: function CL__selectorMatchesElement(aSelector)
  {
    let element = this.viewedElement;
    do {
      if (element.mozMatchesSelector(aSelector)) {
        return true;
      }
    } while ((element = element.parentNode) &&
             element.nodeType === Ci.nsIDOMNode.ELEMENT_NODE);

    return false;
  },

  /**
   * Process the CssSelector object that do not match the highlighted elements,
   * nor its parents. Your callback function is invoked for every such
   * CssSelector object. You receive one argument: the CssSelector object.
   *
   * The list of unmatched selectors is cached.
   *
   * @param {function} aCallback the function you want to execute for each of
   * the unmatched selectors.
   * @param {object} aScope the scope you want for the callback function. aScope
   * will be the this object when aCallback executes.
   */
  processUnmatchedSelectors: function CL_processUnmatchedSelectors(aCallback, aScope)
  {
    if (this._unmatchedSelectors) {
      if (aCallback) {
        this._unmatchedSelectors.forEach(aCallback, aScope);
      }
      return;
    }

    if (!this._matchedSelectors) {
      this.processMatchedSelectors();
    }

    this._unmatchedSelectors = [];

    this.forEachSheet(function (aSheet) {
      // We do not show unmatched selectors from system stylesheets
      if (aSheet.systemSheet || aSheet.disabled || !aSheet.mediaMatches) {
        return;
      }

      aSheet.forEachRule(function (aRule) {
        aRule.selectors.forEach(function (aSelector) {
          if (aSelector._matchId !== this._matchId) {
            this._unmatchedSelectors.push(aSelector);
            if (aCallback) {
              aCallback.call(aScope, aSelector);
            }
          }
        }, this);
      }, this);
    }, this);
  },

  /**
   * Check if the highlighted element or it's parents have matched selectors.
   *
   * @param {array} aProperties The list of properties you want to check if they
   * have matched selectors or not.
   * @return {object} An object that tells for each property if it has matched
   * selectors or not. Object keys are property names and values are booleans.
   */
  hasMatchedSelectors: function CL_hasMatchedSelectors(aProperties)
  {
    if (!this._matchedRules) {
      this._buildMatchedRules();
    }

    let result = {};

    this._matchedRules.some(function(aValue) {
      let rule = aValue[0];
      let status = aValue[1];
      aProperties = aProperties.filter(function(aProperty) {
        // We just need to find if a rule has this property while it matches
        // the viewedElement (or its parents).
        if (rule.getPropertyValue(aProperty) &&
            (status == CssLogic.STATUS.MATCHED ||
             (status == CssLogic.STATUS.PARENT_MATCH &&
              this.domUtils.isInheritedProperty(aProperty)))) {
          result[aProperty] = true;
          return false;
        }
        return true; // Keep the property for the next rule.
      }.bind(this));
      return aProperties.length == 0;
    }, this);

    return result;
  },

  /**
   * Build the array of matched rules for the currently highlighted element.
   * The array will hold rules that match the viewedElement and its parents.
   *
   * @private
   */
  _buildMatchedRules: function CL__buildMatchedRules()
  {
    let domRules;
    let element = this.viewedElement;
    let filter = this.sourceFilter;
    let sheetIndex = 0;

    this._matchId++;
    this._passId++;
    this._matchedRules = [];

    do {
      let status = this.viewedElement === element ?
                   CssLogic.STATUS.MATCHED : CssLogic.STATUS.PARENT_MATCH;

      try {
        domRules = this.domUtils.getCSSStyleRules(element);
      } catch (ex) {
        Services.console.
          logStringMessage("CL__buildMatchedRules error: " + ex);
        continue;
      }

      for (let i = 0, n = domRules.Count(); i < n; i++) {
        let domRule = domRules.GetElementAt(i);
        if (domRule.type !== Ci.nsIDOMCSSRule.STYLE_RULE) {
          continue;
        }

        let sheet = this.getSheet(domRule.parentStyleSheet, -1);
        if (sheet._passId !== this._passId) {
          sheet.index = sheetIndex++;
          sheet._passId = this._passId;
        }

        if (filter !== CssLogic.FILTER.UA && sheet.systemSheet) {
          continue;
        }

        let rule = sheet.getRule(domRule);
        if (rule._passId === this._passId) {
          continue;
        }

        rule._matchId = this._matchId;
        rule._passId = this._passId;
        this._matchedRules.push([rule, status]);
      }


      // Add element.style information.
      if (element.style.length > 0) {
        let rule = new CssRule(null, { style: element.style }, element);
        rule._matchId = this._matchId;
        rule._passId = this._passId;
        this._matchedRules.push([rule, status]);
      }
    } while ((element = element.parentNode) &&
              element.nodeType === Ci.nsIDOMNode.ELEMENT_NODE);
  },

  /**
   * Check if the highlighted element or it's parents have unmatched selectors.
   *
   * Please note that this method is far slower than hasMatchedSelectors()
   * because it needs to do a lot more checks in the DOM.
   *
   * @param {array} aProperties The list of properties you want to check if they
   * have unmatched selectors or not.
   * @return {object} An object that tells for each property if it has unmatched
   * selectors or not. Object keys are property names and values are booleans.
   */
  hasUnmatchedSelectors: function CL_hasUnmatchedSelectors(aProperties)
  {
    if (!this._matchedRules) {
      this._buildMatchedRules();
    }

    let result = {};

    this.forSomeSheets(function (aSheet) {
      if (aSheet.systemSheet || aSheet.disabled || !aSheet.mediaMatches) {
        return false;
      }

      return aSheet.forSomeRules(function (aRule) {
        let unmatched = aRule._matchId !== this._matchId ||
                        this._ruleHasUnmatchedSelector(aRule);
        if (!unmatched) {
          return false;
        }

        aProperties = aProperties.filter(function(aProperty) {
          if (!aRule.getPropertyValue(aProperty)) {
            // Keep this property for the next rule. We need to find a rule
            // which has the property.
            return true;
          }

          result[aProperty] = true;

          // We found a rule that has the current property while it does not
          // match the current element. We can remove this property from the
          // array.
          return false;
        });

        return aProperties.length == 0;
      }, this);
    }, this);

    aProperties.forEach(function(aProperty) { result[aProperty] = false; });

    return result;
  },

  /**
   * Check if a CssRule has an unmatched selector for the highlighted element or
   * its parents.
   *
   * @private
   * @param {CssRule} aRule The rule you want to check if it has an unmatched
   * selector.
   * @return {boolean} True if the rule has an unmatched selector, false
   * otherwise.
   */
  _ruleHasUnmatchedSelector: function CL__ruleHasUnmatchedSelector(aRule)
  {
    if (!aRule._cssSheet && aRule.sourceElement) {
      // CssRule wraps element.style, which never has unmatched selectors.
      return false;
    }

    let element = this.viewedElement;
    let selectors = aRule.selectors;

    do {
      selectors = selectors.filter(function(aSelector) {
        return !element.mozMatchesSelector(aSelector);
      });

      if (selectors.length == 0) {
        break;
      }
    } while ((element = element.parentNode) &&
             element.nodeType === Ci.nsIDOMNode.ELEMENT_NODE);

    return selectors.length > 0;
  },

  /**
   * Tells if the given DOM CSS object matches the current view media.
   *
   * @param {object} aDomObject The DOM CSS object to check.
   * @return {boolean} True if the DOM CSS object matches the current view
   * media, or false otherwise.
   */
  mediaMatches: function CL_mediaMatches(aDomObject)
  {
    let mediaText = aDomObject.media.mediaText;
    return !mediaText || this.viewedDocument.defaultView.
                         matchMedia(mediaText).matches;
   },
};

/**
 * If the element has an id, return '#id'. Otherwise return 'tagname[n]' where
 * n is the index of this element in its siblings.
 * <p>A technically more 'correct' output from the no-id case might be:
 * 'tagname:nth-of-type(n)' however this is unlikely to be more understood
 * and it is longer.
 *
 * @param {nsIDOMElement} aElement the element for which you want the short name.
 * @return {string} the string to be displayed for aElement.
 */
CssLogic.getShortName = function CssLogic_getShortName(aElement)
{
  if (!aElement) {
    return "null";
  }
  if (aElement.id) {
    return "#" + aElement.id;
  }
  let priorSiblings = 0;
  let temp = aElement;
  while (temp = temp.previousElementSibling) {
    priorSiblings++;
  }
  return aElement.tagName + "[" + priorSiblings + "]";
};

/**
 * Get an array of short names from the given element to document.body.
 *
 * @param {nsIDOMElement} aElement the element for which you want the array of
 * short names.
 * @return {array} The array of elements.
 * <p>Each element is an object of the form:
 * <ul>
 * <li>{ display: "what to display for the given (parent) element",
 * <li>  element: referenceToTheElement }
 * </ul>
 */
CssLogic.getShortNamePath = function CssLogic_getShortNamePath(aElement)
{
  let doc = aElement.ownerDocument;
  let reply = [];

  if (!aElement) {
    return reply;
  }

  // We want to exclude nodes high up the tree (body/html) unless the user
  // has selected that node, in which case we need to report something.
  do {
    reply.unshift({
      display: CssLogic.getShortName(aElement),
      element: aElement
    });
    aElement = aElement.parentNode;
  } while (aElement && aElement != doc.body && aElement != doc.head && aElement != doc);

  return reply;
};

/**
 * Memonized lookup of a l10n string from a string bundle.
 * @param {string} aName The key to lookup.
 * @returns A localized version of the given key.
 */
CssLogic.l10n = function(aName) CssLogic._strings.GetStringFromName(aName);

XPCOMUtils.defineLazyGetter(CssLogic, "_strings", function() Services.strings
        .createBundle("chrome://browser/locale/devtools/styleinspector.properties"));

/**
 * Is the given property sheet a system (user agent) stylesheet?
 *
 * @param {CSSStyleSheet} aSheet a stylesheet
 * @return {boolean} true if the given stylesheet is a system stylesheet or
 * false otherwise.
 */
CssLogic.isSystemStyleSheet = function CssLogic_isSystemStyleSheet(aSheet)
{
  if (!aSheet) {
    return true;
  }

  let url = aSheet.href;

  if (!url) return false;
  if (url.length === 0) return true;

  // Check for http[s]
  if (url[0] === 'h') return false;
  if (url.substr(0, 9) === "resource:") return true;
  if (url.substr(0, 7) === "chrome:") return true;
  if (url === "XPCSafeJSObjectWrapper.cpp") return true;
  if (url.substr(0, 6) === "about:") return true;

  return false;
};

/**
 * Return a shortened version of a style sheet's source.
 *
 * @param {CSSStyleSheet} aSheet the DOM object for the style sheet.
 */
CssLogic.shortSource = function CssLogic_shortSource(aSheet)
{
  // Use a string like "inline" if there is no source href
  if (!aSheet || !aSheet.href) {
    return CssLogic.l10n("rule.sourceInline");
  }

  // We try, in turn, the filename, filePath, query string, whole thing
  let url = {};
  try {
    url = Services.io.newURI(aSheet.href, null, null);
    url = url.QueryInterface(Ci.nsIURL);
  } catch (ex) {
    // Some UA-provided stylesheets are not valid URLs.
  }

  if (url.fileName) {
    return url.fileName;
  }

  if (url.filePath) {
    return url.filePath;
  }

  if (url.query) {
    return url.query;
  }

  return aSheet.href;
}

/**
 * A safe way to access cached bits of information about a stylesheet.
 *
 * @constructor
 * @param {CssLogic} aCssLogic pointer to the CssLogic instance working with
 * this CssSheet object.
 * @param {CSSStyleSheet} aDomSheet reference to a DOM CSSStyleSheet object.
 * @param {number} aIndex tells the index/position of the stylesheet within the
 * main document.
 */
function CssSheet(aCssLogic, aDomSheet, aIndex)
{
  this._cssLogic = aCssLogic;
  this.domSheet = aDomSheet;
  this.index = this.systemSheet ? -100 * aIndex : aIndex;

  // Cache of the sheets href. Cached by the getter.
  this._href = null;
  // Short version of href for use in select boxes etc. Cached by getter.
  this._shortSource = null;

  // null for uncached.
  this._sheetAllowed = null;

  // Cached CssRules from the given stylesheet.
  this._rules = {};

  this._ruleCount = -1;
}

CssSheet.prototype = {
  _passId: null,
  _systemSheet: null,
  _mediaMatches: null,

  /**
   * Tells if the stylesheet is provided by the browser or not.
   *
   * @return {boolean} true if this is a browser-provided stylesheet, or false
   * otherwise.
   */
  get systemSheet()
  {
    if (this._systemSheet === null) {
      this._systemSheet = CssLogic.isSystemStyleSheet(this.domSheet);
    }
    return this._systemSheet;
  },

  /**
   * Tells if the stylesheet is disabled or not.
   * @return {boolean} true if this stylesheet is disabled, or false otherwise.
   */
  get disabled()
  {
    return this.domSheet.disabled;
  },

  /**
   * Tells if the stylesheet matches the current browser view media.
   * @return {boolean} true if this stylesheet matches the current browser view
   * media, or false otherwise.
   */
  get mediaMatches()
  {
    if (this._mediaMatches === null) {
      this._mediaMatches = this._cssLogic.mediaMatches(this.domSheet);
    }
    return this._mediaMatches;
  },

  /**
   * Get a source for a stylesheet, taking into account embedded stylesheets
   * for which we need to use document.defaultView.location.href rather than
   * sheet.href
   *
   * @return {string} the address of the stylesheet.
   */
  get href()
  {
    if (!this._href) {
      this._href = this.domSheet.href;
      if (!this._href) {
        this._href = this.domSheet.ownerNode.ownerDocument.location;
      }
    }

    return this._href;
  },

  /**
   * Create a shorthand version of the href of a stylesheet.
   *
   * @return {string} the shorthand source of the stylesheet.
   */
  get shortSource()
  {
    if (this._shortSource) {
      return this._shortSource;
    }

    this._shortSource = CssLogic.shortSource(this.domSheet);
    return this._shortSource;
  },

  /**
   * Tells if the sheet is allowed or not by the current CssLogic.sourceFilter.
   *
   * @return {boolean} true if the stylesheet is allowed by the sourceFilter, or
   * false otherwise.
   */
  get sheetAllowed()
  {
    if (this._sheetAllowed !== null) {
      return this._sheetAllowed;
    }

    this._sheetAllowed = true;

    let filter = this._cssLogic.sourceFilter;
    if (filter === CssLogic.FILTER.ALL && this.systemSheet) {
      this._sheetAllowed = false;
    }
    if (filter !== CssLogic.FILTER.ALL && filter !== CssLogic.FILTER.UA) {
      this._sheetAllowed = (filter === this.href);
    }

    return this._sheetAllowed;
  },

  /**
   * Retrieve the number of rules in this stylesheet.
   *
   * @return {number} the number of nsIDOMCSSRule objects in this stylesheet.
   */
  get ruleCount()
  {
    return this._ruleCount > -1 ?
        this._ruleCount :
        this.domSheet.cssRules.length;
  },

  /**
   * Retrieve a CssRule object for the given CSSStyleRule. The CssRule object is
   * cached, such that subsequent retrievals return the same CssRule object for
   * the same CSSStyleRule object.
   *
   * @param {CSSStyleRule} aDomRule the CSSStyleRule object for which you want a
   * CssRule object.
   * @return {CssRule} the cached CssRule object for the given CSSStyleRule
   * object.
   */
  getRule: function CssSheet_getRule(aDomRule)
  {
    let cacheId = aDomRule.type + aDomRule.selectorText;

    let rule = null;
    let ruleFound = false;

    if (cacheId in this._rules) {
      for (let i = 0, rulesLen = this._rules[cacheId].length; i < rulesLen; i++) {
        rule = this._rules[cacheId][i];
        if (rule._domRule === aDomRule) {
          ruleFound = true;
          break;
        }
      }
    }

    if (!ruleFound) {
      if (!(cacheId in this._rules)) {
        this._rules[cacheId] = [];
      }

      rule = new CssRule(this, aDomRule);
      this._rules[cacheId].push(rule);
    }

    return rule;
  },

  /**
   * Process each rule in this stylesheet using your callback function. Your
   * function receives one argument: the CssRule object for each CSSStyleRule
   * inside the stylesheet.
   *
   * Note that this method also iterates through @media rules inside the
   * stylesheet.
   *
   * @param {function} aCallback the function you want to execute for each of
   * the style rules.
   * @param {object} aScope the scope you want for the callback function. aScope
   * will be the this object when aCallback executes.
   */
  forEachRule: function CssSheet_forEachRule(aCallback, aScope)
  {
    let ruleCount = 0;
    let domRules = this.domSheet.cssRules;

    function _iterator(aDomRule) {
      if (aDomRule.type == Ci.nsIDOMCSSRule.STYLE_RULE) {
        aCallback.call(aScope, this.getRule(aDomRule));
        ruleCount++;
      } else if (aDomRule.type == Ci.nsIDOMCSSRule.MEDIA_RULE &&
          aDomRule.cssRules && this._cssLogic.mediaMatches(aDomRule)) {
        Array.prototype.forEach.call(aDomRule.cssRules, _iterator, this);
      }
    }

    Array.prototype.forEach.call(domRules, _iterator, this);

    this._ruleCount = ruleCount;
  },

  /**
   * Process *some* rules in this stylesheet using your callback function. Your
   * function receives one argument: the CssRule object for each CSSStyleRule
   * inside the stylesheet. In order to stop processing the callback function
   * needs to return a value.
   *
   * Note that this method also iterates through @media rules inside the
   * stylesheet.
   *
   * @param {function} aCallback the function you want to execute for each of
   * the style rules.
   * @param {object} aScope the scope you want for the callback function. aScope
   * will be the this object when aCallback executes.
   * @return {Boolean} true if aCallback returns true during any iteration,
   * otherwise false is returned.
   */
  forSomeRules: function CssSheet_forSomeRules(aCallback, aScope)
  {
    let domRules = this.domSheet.cssRules;
    function _iterator(aDomRule) {
      if (aDomRule.type == Ci.nsIDOMCSSRule.STYLE_RULE) {
        return aCallback.call(aScope, this.getRule(aDomRule));
      } else if (aDomRule.type == Ci.nsIDOMCSSRule.MEDIA_RULE &&
          aDomRule.cssRules && this._cssLogic.mediaMatches(aDomRule)) {
        return Array.prototype.some.call(aDomRule.cssRules, _iterator, this);
      }
    }
    return Array.prototype.some.call(domRules, _iterator, this);
  },

  toString: function CssSheet_toString()
  {
    return "CssSheet[" + this.shortSource + "]";
  },
};

/**
 * Information about a single CSSStyleRule.
 *
 * @param {CSSSheet|null} aCssSheet the CssSheet object of the stylesheet that
 * holds the CSSStyleRule. If the rule comes from element.style, set this
 * argument to null.
 * @param {CSSStyleRule|object} aDomRule the DOM CSSStyleRule for which you want
 * to cache data. If the rule comes from element.style, then provide
 * an object of the form: {style: element.style}.
 * @param {Element} [aElement] If the rule comes from element.style, then this
 * argument must point to the element.
 * @constructor
 */
function CssRule(aCssSheet, aDomRule, aElement)
{
  this._cssSheet = aCssSheet;
  this._domRule = aDomRule;

  if (this._cssSheet) {
    // parse _domRule.selectorText on call to this.selectors
    this._selectors = null;
    this.line = this._cssSheet._cssLogic.domUtils.getRuleLine(this._domRule);
    this.source = this._cssSheet.shortSource + ":" + this.line;
    this.href = this._cssSheet.href;
    this.systemRule = this._cssSheet.systemSheet;
  } else if (aElement) {
    this._selectors = [ new CssSelector(this, "@element.style") ];
    this.line = -1;
    this.source = CssLogic.l10n("rule.sourceElement");
    this.href = "#";
    this.systemRule = false;
    this.sourceElement = aElement;
  }
}

CssRule.prototype = {
  _passId: null,

  /**
   * Check if the parent stylesheet is allowed by the CssLogic.sourceFilter.
   *
   * @return {boolean} true if the parent stylesheet is allowed by the current
   * sourceFilter, or false otherwise.
   */
  get sheetAllowed()
  {
    return this._cssSheet ? this._cssSheet.sheetAllowed : true;
  },

  /**
   * Retrieve the parent stylesheet index/position in the viewed document.
   *
   * @return {number} the parent stylesheet index/position in the viewed
   * document.
   */
  get sheetIndex()
  {
    return this._cssSheet ? this._cssSheet.index : 0;
  },

  /**
   * Retrieve the style property value from the current CSSStyleRule.
   *
   * @param {string} aProperty the CSS property name for which you want the
   * value.
   * @return {string} the property value.
   */
  getPropertyValue: function(aProperty)
  {
    return this._domRule.style.getPropertyValue(aProperty);
  },

  /**
   * Retrieve the style property priority from the current CSSStyleRule.
   *
   * @param {string} aProperty the CSS property name for which you want the
   * priority.
   * @return {string} the property priority.
   */
  getPropertyPriority: function(aProperty)
  {
    return this._domRule.style.getPropertyPriority(aProperty);
  },

  /**
   * Retrieve the list of CssSelector objects for each of the parsed selectors
   * of the current CSSStyleRule.
   *
   * @return {array} the array hold the CssSelector objects.
   */
  get selectors()
  {
    if (this._selectors) {
      return this._selectors;
    }

    // Parse the CSSStyleRule.selectorText string.
    this._selectors = [];

    if (!this._domRule.selectorText) {
      return this._selectors;
    }

    let selector = this._domRule.selectorText.trim();
    if (!selector) {
      return this._selectors;
    }

    let nesting = 0;
    let currentSelector = [];

    // Parse a selector group into selectors. Normally we could just .split(',')
    // however Gecko allows -moz-any(a, b, c) as a selector so we ignore commas
    // inside brackets.
    for (let i = 0, selLen = selector.length; i < selLen; i++) {
      let c = selector.charAt(i);
      switch (c) {
        case ",":
          if (nesting == 0 && currentSelector.length > 0) {
            let selectorStr = currentSelector.join("").trim();
            if (selectorStr) {
              this._selectors.push(new CssSelector(this, selectorStr));
            }
            currentSelector = [];
          } else {
            currentSelector.push(c);
          }
          break;

        case "(":
          nesting++;
          currentSelector.push(c);
          break;

        case ")":
          nesting--;
          currentSelector.push(c);
          break;

        default:
          currentSelector.push(c);
          break;
      }
    }

    // Add the last selector.
    if (nesting == 0 && currentSelector.length > 0) {
      let selectorStr = currentSelector.join("").trim();
      if (selectorStr) {
        this._selectors.push(new CssSelector(this, selectorStr));
      }
    }

    return this._selectors;
  },

  toString: function CssRule_toString()
  {
    return "[CssRule " + this._domRule.selectorText + "]";
  },
};

/**
 * The CSS selector class allows us to document the ranking of various CSS
 * selectors.
 *
 * @constructor
 * @param {CssRule} aCssRule the CssRule instance from where the selector comes.
 * @param {string} aSelector The selector that we wish to investigate.
 */
function CssSelector(aCssRule, aSelector)
{
  this._cssRule = aCssRule;
  this.text = aSelector;
  this.elementStyle = this.text == "@element.style";
  this._specificity = null;
}

CssSelector.prototype = {
  _matchId: null,

  /**
   * Retrieve the CssSelector source, which is the source of the CssSheet owning
   * the selector.
   *
   * @return {string} the selector source.
   */
  get source()
  {
    return this._cssRule.source;
  },

  /**
   * Retrieve the CssSelector source element, which is the source of the CssRule
   * owning the selector. This is only available when the CssSelector comes from
   * an element.style.
   *
   * @return {string} the source element selector.
   */
  get sourceElement()
  {
    return this._cssRule.sourceElement;
  },

  /**
   * Retrieve the address of the CssSelector. This points to the address of the
   * CssSheet owning this selector.
   *
   * @return {string} the address of the CssSelector.
   */
  get href()
  {
    return this._cssRule.href;
  },

  /**
   * Check if the selector comes from a browser-provided stylesheet.
   *
   * @return {boolean} true if the selector comes from a browser-provided
   * stylesheet, or false otherwise.
   */
  get systemRule()
  {
    return this._cssRule.systemRule;
  },

  /**
   * Check if the parent stylesheet is allowed by the CssLogic.sourceFilter.
   *
   * @return {boolean} true if the parent stylesheet is allowed by the current
   * sourceFilter, or false otherwise.
   */
  get sheetAllowed()
  {
    return this._cssRule.sheetAllowed;
  },

  /**
   * Retrieve the parent stylesheet index/position in the viewed document.
   *
   * @return {number} the parent stylesheet index/position in the viewed
   * document.
   */
  get sheetIndex()
  {
    return this._cssRule.sheetIndex;
  },

  /**
   * Retrieve the line of the parent CSSStyleRule in the parent CSSStyleSheet.
   *
   * @return {number} the line of the parent CSSStyleRule in the parent
   * stylesheet.
   */
  get ruleLine()
  {
    return this._cssRule.line;
  },

  /**
   * Retrieve specificity information for the current selector.
   *
   * @see http://www.w3.org/TR/css3-selectors/#specificity
   * @see http://www.w3.org/TR/CSS2/selector.html
   *
   * @return {object} an object holding specificity information for the current
   * selector.
   */
  get specificity()
  {
    if (this._specificity) {
      return this._specificity;
    }

    let specificity = {};

    specificity.ids = 0;
    specificity.classes = 0;
    specificity.tags = 0;

    // Split on CSS combinators (section 5.2).
    // TODO: We need to properly parse the selector. See bug 592743.
    if (!this.elementStyle) {
      this.text.split(/[ >+]/).forEach(function(aSimple) {
        // The regex leaves empty nodes combinators like ' > '
        if (!aSimple) {
          return;
        }
        // See http://www.w3.org/TR/css3-selectors/#specificity
        // We can count the IDs by counting the '#' marks.
        specificity.ids += (aSimple.match(/#/g) || []).length;
        // Similar with class names and attribute matchers
        specificity.classes += (aSimple.match(/\./g) || []).length;
        specificity.classes += (aSimple.match(/\[/g) || []).length;
        // Pseudo elements count as elements.
        specificity.tags += (aSimple.match(/:/g) || []).length;
        // If we have anything of substance before we get into ids/classes/etc
        // then it must be a tag if it isn't '*'.
        let tag = aSimple.split(/[#.[:]/)[0];
        if (tag && tag != "*") {
          specificity.tags++;
        }
      }, this);
    }

    this._specificity = specificity;

    return this._specificity;
  },

  toString: function CssSelector_toString()
  {
    return this.text;
  },
};

/**
 * A cache of information about the matched rules, selectors and values attached
 * to a CSS property, for the highlighted element.
 *
 * The heart of the CssPropertyInfo object is the _findMatchedSelectors() and
 * _findUnmatchedSelectors() methods. These are invoked when the PropertyView
 * tries to access the .matchedSelectors and .unmatchedSelectors arrays.
 * Results are cached, for later reuse.
 *
 * @param {CssLogic} aCssLogic Reference to the parent CssLogic instance
 * @param {string} aProperty The CSS property we are gathering information for
 * @constructor
 */
function CssPropertyInfo(aCssLogic, aProperty)
{
  this._cssLogic = aCssLogic;
  this.property = aProperty;
  this._value = "";

  // The number of matched rules holding the this.property style property.
  // Additionally, only rules that come from allowed stylesheets are counted.
  this._matchedRuleCount = 0;

  // An array holding CssSelectorInfo objects for each of the matched selectors
  // that are inside a CSS rule. Only rules that hold the this.property are
  // counted. This includes rules that come from filtered stylesheets (those
  // that have sheetAllowed = false).
  this._matchedSelectors = null;
  this._unmatchedSelectors = null;
}

CssPropertyInfo.prototype = {
  /**
   * Retrieve the computed style value for the current property, for the
   * highlighted element.
   *
   * @return {string} the computed style value for the current property, for the
   * highlighted element.
   */
  get value()
  {
    if (!this._value && this._cssLogic._computedStyle) {
      try {
        this._value = this._cssLogic._computedStyle.getPropertyValue(this.property);
      } catch (ex) {
        Services.console.logStringMessage('Error reading computed style for ' +
          this.property);
        Services.console.logStringMessage(ex);
      }
    }

    return this._value;
  },

  /**
   * Retrieve the number of matched rules holding the this.property style
   * property. Only rules that come from allowed stylesheets are counted.
   *
   * @return {number} the number of matched rules.
   */
  get matchedRuleCount()
  {
    if (!this._matchedSelectors) {
      this._findMatchedSelectors();
    } else if (this.needRefilter) {
      this._refilterSelectors();
    }

    return this._matchedRuleCount;
  },

  /**
   * Retrieve the number of unmatched rules.
   *
   * @return {number} the number of rules that do not match the highlighted
   * element or its parents.
   */
  get unmatchedRuleCount()
  {
    if (!this._unmatchedSelectors) {
      this._findUnmatchedSelectors();
    } else if (this.needRefilter) {
      this._refilterSelectors();
    }

    return this._unmatchedRuleCount;
  },

  /**
   * Retrieve the array holding CssSelectorInfo objects for each of the matched
   * selectors, from each of the matched rules. Only selectors coming from
   * allowed stylesheets are included in the array.
   *
   * @return {array} the list of CssSelectorInfo objects of selectors that match
   * the highlighted element and its parents.
   */
  get matchedSelectors()
  {
    if (!this._matchedSelectors) {
      this._findMatchedSelectors();
    } else if (this.needRefilter) {
      this._refilterSelectors();
    }

    return this._matchedSelectors;
  },

  /**
   * Retrieve the array holding CssSelectorInfo objects for each of the
   * unmatched selectors, from each of the unmatched rules. Only selectors
   * coming from allowed stylesheets are included in the array.
   *
   * @return {array} the list of CssSelectorInfo objects of selectors that do
   * not match the highlighted element or its parents.
   */
  get unmatchedSelectors()
  {
    if (!this._unmatchedSelectors) {
      this._findUnmatchedSelectors();
    } else if (this.needRefilter) {
      this._refilterSelectors();
    }

    return this._unmatchedSelectors;
  },

  /**
   * Find the selectors that match the highlighted element and its parents.
   * Uses CssLogic.processMatchedSelectors() to find the matched selectors,
   * passing in a reference to CssPropertyInfo._processMatchedSelector() to
   * create CssSelectorInfo objects, which we then sort
   * @private
   */
  _findMatchedSelectors: function CssPropertyInfo_findMatchedSelectors()
  {
    this._matchedSelectors = [];
    this._matchedRuleCount = 0;
    this.needRefilter = false;

    this._cssLogic.processMatchedSelectors(this._processMatchedSelector, this);

    // Sort the selectors by how well they match the given element.
    this._matchedSelectors.sort(function(aSelectorInfo1, aSelectorInfo2) {
      if (aSelectorInfo1.status > aSelectorInfo2.status) {
        return -1;
      } else if (aSelectorInfo2.status > aSelectorInfo1.status) {
        return 1;
      } else {
        return aSelectorInfo1.compareTo(aSelectorInfo2);
      }
    });

    // Now we know which of the matches is best, we can mark it BEST_MATCH.
    if (this._matchedSelectors.length > 0 &&
        this._matchedSelectors[0].status > CssLogic.STATUS.UNMATCHED) {
      this._matchedSelectors[0].status = CssLogic.STATUS.BEST;
    }
  },

  /**
   * Process a matched CssSelector object.
   *
   * @private
   * @param {CssSelector} aSelector the matched CssSelector object.
   * @param {CssLogic.STATUS} aStatus the CssSelector match status.
   */
  _processMatchedSelector: function CssPropertyInfo_processMatchedSelector(aSelector, aStatus)
  {
    let cssRule = aSelector._cssRule;
    let value = cssRule.getPropertyValue(this.property);
    if (value &&
        (aStatus == CssLogic.STATUS.MATCHED ||
         (aStatus == CssLogic.STATUS.PARENT_MATCH &&
          this._cssLogic.domUtils.isInheritedProperty(this.property)))) {
      let selectorInfo = new CssSelectorInfo(aSelector, this.property, value,
          aStatus);
      this._matchedSelectors.push(selectorInfo);
      if (this._cssLogic._passId !== cssRule._passId && cssRule.sheetAllowed) {
        this._matchedRuleCount++;
      }
    }
  },

  /**
   * Find the selectors that do not match the highlighted element and its
   * parents.
   * @private
   */
  _findUnmatchedSelectors: function CssPropertyInfo_findUnmatchedSelectors()
  {
    this._unmatchedSelectors = [];
    this._unmatchedRuleCount = 0;
    this.needRefilter = false;
    this._cssLogic._passId++;

    this._cssLogic.processUnmatchedSelectors(this._processUnmatchedSelector,
        this);

    // Sort the selectors by specificity.
    this._unmatchedSelectors.sort(function(aSelectorInfo1, aSelectorInfo2) {
      return aSelectorInfo1.compareTo(aSelectorInfo2);
    });
  },

  /**
   * Process an unmatched CssSelector object. Note that in order to avoid
   * information overload we DO NOT show unmatched system rules.
   *
   * @private
   * @param {CssSelector} aSelector the unmatched CssSelector object.
   */
  _processUnmatchedSelector: function CPI_processUnmatchedSelector(aSelector)
  {
    let cssRule = aSelector._cssRule;
    let value = cssRule.getPropertyValue(this.property);
    if (value) {
      let selectorInfo = new CssSelectorInfo(aSelector, this.property, value,
          CssLogic.STATUS.UNMATCHED);
      this._unmatchedSelectors.push(selectorInfo);
      if (this._cssLogic._passId != cssRule._passId) {
        if (cssRule.sheetAllowed) {
          this._unmatchedRuleCount++;
        }
        cssRule._passId = this._cssLogic._passId;
      }
    }
  },

  /**
   * Refilter the matched and unmatched selectors arrays when the
   * CssLogic.sourceFilter changes. This allows for quick filter changes.
   * @private
   */
  _refilterSelectors: function CssPropertyInfo_refilterSelectors()
  {
    let passId = ++this._cssLogic._passId;
    let ruleCount = 0;

    let iterator = function(aSelectorInfo) {
      let cssRule = aSelectorInfo.selector._cssRule;
      if (cssRule._passId != passId) {
        if (cssRule.sheetAllowed) {
          ruleCount++;
        }
        cssRule._passId = passId;
      }
    };

    if (this._matchedSelectors) {
      this._matchedSelectors.forEach(iterator);
      this._matchedRuleCount = ruleCount;
    }

    if (this._unmatchedSelectors) {
      ruleCount = 0;
      this._unmatchedSelectors.forEach(iterator);
      this._unmatchedRuleCount = ruleCount;
    }

    this.needRefilter = false;
  },

  toString: function CssPropertyInfo_toString()
  {
    return "CssPropertyInfo[" + this.property + "]";
  },
};

/**
 * A class that holds information about a given CssSelector object.
 *
 * Instances of this class are given to CssHtmlTree in the arrays of matched and
 * unmatched selectors. Each such object represents a displayable row in the
 * PropertyView objects. The information given by this object blends data coming
 * from the CssSheet, CssRule and from the CssSelector that own this object.
 *
 * @param {CssSelector} aSelector The CssSelector object for which to present information.
 * @param {string} aProperty The property for which information should be retrieved.
 * @param {string} aValue The property value from the CssRule that owns the selector.
 * @param {CssLogic.STATUS} aStatus The selector match status.
 * @constructor
 */
function CssSelectorInfo(aSelector, aProperty, aValue, aStatus)
{
  this.selector = aSelector;
  this.property = aProperty;
  this.value = aValue;
  this.status = aStatus;

  let priority = this.selector._cssRule.getPropertyPriority(this.property);
  this.important = (priority === "important");

  /* Score prefix:
  0 UA normal property
  1 UA important property
  2 normal property
  3 inline (element.style)
  4 important
  5 inline important
  */
  let scorePrefix = this.systemRule ? 0 : 2;
  if (this.elementStyle) {
    scorePrefix++;
  }
  if (this.important) {
    scorePrefix += this.systemRule ? 1 : 2;
  }

  this.specificityScore = "" + scorePrefix + this.specificity.ids +
      this.specificity.classes + this.specificity.tags;
}

CssSelectorInfo.prototype = {
  /**
   * Retrieve the CssSelector source, which is the source of the CssSheet owning
   * the selector.
   *
   * @return {string} the selector source.
   */
  get source()
  {
    return this.selector.source;
  },

  /**
   * Retrieve the CssSelector source element, which is the source of the CssRule
   * owning the selector. This is only available when the CssSelector comes from
   * an element.style.
   *
   * @return {string} the source element selector.
   */
  get sourceElement()
  {
    return this.selector.sourceElement;
  },

  /**
   * Retrieve the address of the CssSelector. This points to the address of the
   * CssSheet owning this selector.
   *
   * @return {string} the address of the CssSelector.
   */
  get href()
  {
    return this.selector.href;
  },

  /**
   * Check if the CssSelector comes from element.style or not.
   *
   * @return {boolean} true if the CssSelector comes from element.style, or
   * false otherwise.
   */
  get elementStyle()
  {
    return this.selector.elementStyle;
  },

  /**
   * Retrieve specificity information for the current selector.
   *
   * @return {object} an object holding specificity information for the current
   * selector.
   */
  get specificity()
  {
    return this.selector.specificity;
  },

  /**
   * Retrieve the parent stylesheet index/position in the viewed document.
   *
   * @return {number} the parent stylesheet index/position in the viewed
   * document.
   */
  get sheetIndex()
  {
    return this.selector.sheetIndex;
  },

  /**
   * Check if the parent stylesheet is allowed by the CssLogic.sourceFilter.
   *
   * @return {boolean} true if the parent stylesheet is allowed by the current
   * sourceFilter, or false otherwise.
   */
  get sheetAllowed()
  {
    return this.selector.sheetAllowed;
  },

  /**
   * Retrieve the line of the parent CSSStyleRule in the parent CSSStyleSheet.
   *
   * @return {number} the line of the parent CSSStyleRule in the parent
   * stylesheet.
   */
  get ruleLine()
  {
    return this.selector.ruleLine;
  },

  /**
   * Check if the selector comes from a browser-provided stylesheet.
   *
   * @return {boolean} true if the selector comes from a browser-provided
   * stylesheet, or false otherwise.
   */
  get systemRule()
  {
    return this.selector.systemRule;
  },

  /**
   * Compare the current CssSelectorInfo instance to another instance, based on
   * specificity information.
   *
   * @param {CssSelectorInfo} aThat The instance to compare ourselves against.
   * @return number -1, 0, 1 depending on how aThat compares with this.
   */
  compareTo: function CssSelectorInfo_compareTo(aThat)
  {
    if (this.systemRule && !aThat.systemRule) return 1;
    if (!this.systemRule && aThat.systemRule) return -1;

    if (this.elementStyle && !aThat.elementStyle) {
      if (!this.important && aThat.important) return 1;
      else return -1;
    }

    if (!this.elementStyle && aThat.elementStyle) {
      if (this.important && !aThat.important) return -1;
      else return 1;
    }

    if (this.important && !aThat.important) return -1;
    if (aThat.important && !this.important) return 1;

    if (this.specificity.ids > aThat.specificity.ids) return -1;
    if (aThat.specificity.ids > this.specificity.ids) return 1;

    if (this.specificity.classes > aThat.specificity.classes) return -1;
    if (aThat.specificity.classes > this.specificity.classes) return 1;

    if (this.specificity.tags > aThat.specificity.tags) return -1;
    if (aThat.specificity.tags > this.specificity.tags) return 1;

    if (this.sheetIndex > aThat.sheetIndex) return -1;
    if (aThat.sheetIndex > this.sheetIndex) return 1;

    if (this.ruleLine > aThat.ruleLine) return -1;
    if (aThat.ruleLine > this.ruleLine) return 1;

    return 0;
  },

  toString: function CssSelectorInfo_toString()
  {
    return this.selector + " -> " + this.value;
  },
};
