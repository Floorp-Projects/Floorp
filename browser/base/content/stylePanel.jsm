/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2007, Parakey Inc.
 * All rights reserved.
 * 
 * Redistribution and use of this software in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * * Redistributions of source code must retain the above
 *   copyright notice, this list of conditions and the
 *   following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the
 *   following disclaimer in the documentation and/or other
 *   materials provided with the distribution.
 * 
 * * Neither the name of Parakey Inc. nor the names of its
 *   contributors may be used to endorse or promote products
 *   derived from this software without specific prior
 *   written permission of Parakey Inc.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Creator:
 *  Joe Hewitt
 * Contributors
 *  John J. Barton (IBM Almaden)
 *  Jan Odvarko (Mozilla Corp.)
 *  Max Stepanov (Aptana Inc.)
 *  Rob Campbell (Mozilla Corp.)
 *  Hans Hillen (Paciello Group, Mozilla)
 *  Curtis Bartley (Mozilla Corp.)
 *  Mike Collins (IBM Almaden)
 *  Kevin Decker
 *  Mike Ratcliffe (Comartis AG)
 *  Hernan Rodríguez Colmeiro
 *  Austin Andrews
 *  Christoph Dorn
 *  Steven Roussey (AppCenter Inc, Network54)
 */

var EXPORTED_SYMBOLS = ["style"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

var style = {

  /**
   * initialize domUtils
   */
  initialize: function CSS_initialize()
  {
    this.domUtils = Cc["@mozilla.org/inspector/dom-utils;1"].
      getService(Ci["inIDOMUtils"]);
  },

  /**
   * Is the given property sheet a system (user agent) stylesheet?
   *
   * @param aSheet
   *        a stylesheet
   */
  isSystemStyleSheet: function CSS_isSystemStyleSheet(aSheet)
  {
    if (!aSheet)
      return true;

    let url = aSheet.href;

    if (!url)
      return false;
    if (url.length == 0)
      return true;
    if (url[0] == 'h') 
      return false;
    if (url.substr(0, 9) == "resource:")
      return true;
    if (url.substr(0, 7) == "chrome:")
      return true;
    if (url  == "XPCSafeJSObjectWrapper.cpp")
      return true;
    if (url.substr(0, 6) == "about:")
      return true;

    return false;
  },

  /**
   * Parse properties from a given style object.
   * Borrowed from Firebug's css.js.
   *
   * @param aStyle
   *        a style object
   */
  parseCSSProperties: function CSS_parseCSSProps(aStyle)
  {
    let properties = [];
    let lines = aStyle.cssText.match(/(?:[^;\(]*(?:\([^\)]*?\))?[^;\(]*)*;?/g);
    let propRE = /\s*([^:\s]*)\s*:\s*(.*?)\s*(! important)?;?$/;
    let line, i = 0;
    while(line = lines[i++]) {
      let match = propRE.exec(line);
      if (!match)
        continue;
      let name = match[1];
      let value = match[2];
      let important = !!match[3]; // true if match[3] is non-empty
      properties.unshift({name: name, value: value, important: important});
    }

    return properties;
  },

  /**
   * Mark properties overridden further up the hierarchy.
   *
   * @param aProps
   *        Array of properties.
   * @param aUsedProps
   *        Object of arrays keyed by property name.
   * @param aInherit
   *        Boolean of whether or not we are in inherited mode.
   */
  markOverriddenProperties: function CSS_markOverriddenProperties(aProps, aUsedProps, aInherit)
  {
    for (let i = 0; i < aProps.length; ++i) {
      let prop = aProps[i];
      if (aUsedProps.hasOwnProperty(prop.name)) {
        // all previous occurrences of this property
        let deadProps = aUsedProps[prop.name];
        for (let j = 0; j < deadProps.length; ++j) {
          let deadProp = deadProps[j];
          if (!deadProp.disabled && !deadProp.wasInherited &&
              deadProp.important && !prop.important) {
            prop.overridden = true;  // new occurrence overridden
          } else if (!prop.disabled) {
            deadProp.overridden = true;  // previous occurrences overridden
          } else {
            aUsedProps[prop.name] = [];
          }

          prop.wasInherited = aInherit ? true : false;
          // all occurrences of a property seen so far, by name
          aUsedProps[prop.name].push(prop);
        }
      }
    }
  },

  /**
   * Sort given properties in lexical order by name.
   *
   * @param properties
   *        An array of properties.
   * @returns sorted array.
   */
  sortProperties: function CSS_sortProperties(properties)
  {
    properties.sort(function(a, b)
    {
      if (a.name < b.name) {
        return -1;
      }
      if (a.name > b.name) {
        return 1;
      }
      return 0;
    });
  },

  /**
   * Get properties for a given element and push them to the rules array.
   *
   * @param aNode
   *        a DOM node
   * @param rules
   *        An array of rules to add properties to.
   * @param usedProps
   *        Object of arrays keyed by property name.
   * @param inherit
   *        boolean determining whether or not we're in inherit mode
   */
  getStyleProperties: function CSS_getStyleProperties(aNode, aRules, aUsedProps, aInherit)
  {
    let properties = this.parseCSSProperties(aNode.style, aInherit);

    this.sortProperties(properties);
    this.markOverriddenProperties(properties, aUsedProps, aInherit);

    if (properties.length) {
      aRules.push({rule: aNode, selector: "element.style",
        properties: properties, inherited: aInherit});
    }
  },

  /**
   * Get properties for a given rule.
   *
   * @param aRule
   *        A Rule from a stylesheet.
   */
  getRuleProperties: function CSS_getRuleProperties(aRule)
  {
    let style = aRule.style;
    return this.parseCSSProperties(style);
  },

  /**
   * Recursively get rules for an element's parents and add them to the
   * sections array.
   *
   * @param aNode
   *        an element in a DOM tree.
   * @param sections
   *        an array of sections
   * @param usedProps
   *        Object of arrays keyed by property name.
   */
  getInheritedRules: function CSS_getInheritedRules(aNode, aSections, aUsedProps)
  {
    let parent = aNode.parentNode;
    if (parent && parent.nodeType == 1) {
      this.getInheritedRules(parent, aSections, aUsedProps);

      let rules = [];
      this.getElementRules(parent, rules, aUsedProps, true);

      if (rules.length) {
        aSections.unshift({element: parent, rules: rules});
      }
    }
  },

  /**
   * Get the CSS style rules for a given node in the DOM and append them to the
   * rules array.
   *
   * @param aNode
   *        an element in the DOM tree.
   * @param aRules
   *        an array of rules.
   * @param aUsedProps
   *        Object of arrays keyed by property name.
   * @param aInherit
   *        boolean indicating whether we are in an inherited mode or not
   */
  getElementRules: function CSS_getElementRules(aNode, aRules, aUsedProps, aInherit)
  {
    let inspectedRules;

    try {
      inspectedRules = this.domUtils.getCSSStyleRules(aNode);
    } catch (ex) {
      Services.console.logStringMessage(ex);
    }

    if (!inspectedRules)
      return;

    for (let i = 0; i < inspectedRules.Count(); ++i) {
      let rule = inspectedRules.GetElementAt(i);
      let href = rule.parentStyleSheet.href;

      if (!href) {
        // Null href means inline style.
        href = aNode.ownerDocument.location.href;
      }

      let isSystemSheet = this.isSystemStyleSheet(rule.parentStyleSheet);

      if (isSystemSheet)
        continue;

      let properties = this.getRuleProperties(rule, aInherit);
      if (aInherit && !properties.length)
        continue;

      let line = this.domUtils.getRuleLine(rule);
      let ruleId = rule.selectorText + " " + href + " (" + line + ")";

      let sourceLink = "view-source:" + href + "#" + line;

      this.markOverriddenProperties(properties, aUsedProps, aInherit);

      aRules.unshift(
        {rule: rule,
         id: ruleId,
         selector: rule.selectorText,
         properties: properties,
         inherited: aInherit,
         sourceLink: sourceLink,
         isSystemSheet: isSystemSheet});
    }

    if (aNode.style) {
      this.getStyleProperties(aNode, aRules, aUsedProps, aInherit);
    }
  },
};

