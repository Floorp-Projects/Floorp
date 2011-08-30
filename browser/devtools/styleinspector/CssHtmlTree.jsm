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

var EXPORTED_SYMBOLS = ["CssHtmlTree"];

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

  // The document in which we display the results (csshtmltree.xhtml).
  this.styleDocument = this.styleWin.contentWindow.document;

  // Nodes used in templating
  this.root = this.styleDocument.getElementById("root");
  this.templateRoot = this.styleDocument.getElementById("templateRoot");
  this.panel = aPanel;

  // The element that we're inspecting, and the document that it comes from.
  this.viewedElement = null;
  this.viewedDocument = null;

  this.createStyleGroupViews();
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
 */
CssHtmlTree.processTemplate = function CssHtmlTree_processTemplate(aTemplate, aDestination, aData)
{
  aDestination.innerHTML = "";

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
  /**
   * Focus the output display on a specific element.
   * @param {nsIDOMElement} aElement The highlighted node to get styles for.
   */
  highlight: function CssHtmlTree_highlight(aElement)
  {
    this.viewedElement = aElement;

    // Reset the style groups. Without this previously expanded groups
    // will fail to expand when inspecting subsequent nodes
    let close = !aElement;
    this.styleGroups.forEach(function(group) group.reset(close));

    if (this.viewedElement) {
      this.viewedDocument = this.viewedElement.ownerDocument;
      CssHtmlTree.processTemplate(this.templateRoot, this.root, this);
    } else {
      this.viewedDocument = null;
      this.root.innerHTML = "";
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
    if (aEvent.target && aEvent.target.pathElement) {
      if (this.win.InspectorUI.selection) {
        if (aEvent.target.pathElement != this.win.InspectorUI.selection) {
          this.win.InspectorUI.inspectNode(aEvent.target.pathElement);
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
   * Returns arrays of categorized properties.
   */
  _getPropertiesByGroup: function CssHtmlTree_getPropertiesByGroup()
  {
    return {
      text: [
        "color",                    // inherit http://www.w3.org/TR/CSS21/propidx.html
        "color-interpolation",      //
        "color-interpolation-filters", //
        "direction",                // inherit http://www.w3.org/TR/CSS21/propidx.html
        "fill",                     //
        "fill-opacity",             //
        "fill-rule",                //
        "filter",                   //
        "flood-color",              //
        "flood-opacity",            //
        "font-family",              // inherit http://www.w3.org/TR/CSS21/propidx.html
        "font-size",                // inherit http://www.w3.org/TR/CSS21/propidx.html
        "font-size-adjust",         // inherit http://www.w3.org/TR/WD-font/#font-size-props
        "font-stretch",             // inherit http://www.w3.org/TR/WD-font/#font-stretch
        "font-style",               // inherit http://www.w3.org/TR/CSS21/propidx.html
        "font-variant",             // inherit http://www.w3.org/TR/CSS21/propidx.html
        "font-weight",              // inherit http://www.w3.org/TR/CSS21/propidx.html
        "ime-mode",                 //
        "letter-spacing",           // inherit http://www.w3.org/TR/CSS21/propidx.html
        "lighting-color",           //
        "line-height",              // inherit http://www.w3.org/TR/CSS21/propidx.html
        "opacity",                  // no      http://www.w3.org/TR/css3-color/#transparency
        "quotes",                   // inherit http://www.w3.org/TR/CSS21/propidx.html
        "stop-color",               //
        "stop-opacity",             //
        "stroke-opacity",           //
        "text-align",               // inherit http://www.w3.org/TR/CSS21/propidx.html
        "text-anchor",              //
        "text-decoration",          // no      http://www.w3.org/TR/CSS21/propidx.html
        "text-indent",              // inherit http://www.w3.org/TR/CSS21/propidx.html
        "text-overflow",            //
        "text-rendering",           // inherit http://www.w3.org/TR/SVG/painting.html#TextRenderingProperty !
        "text-shadow",              // inherit http://www.w3.org/TR/css3-text/#text-shadow
        "text-transform",           // inherit http://www.w3.org/TR/CSS21/propidx.html
        "vertical-align",           // no      http://www.w3.org/TR/CSS21/propidx.html
        "white-space",              // inherit http://www.w3.org/TR/CSS21/propidx.html
        "word-spacing",             // inherit http://www.w3.org/TR/css3-text/#word-spacing
        "word-wrap",                // inherit http://www.w3.org/TR/css3-text/#word-wrap
        "-moz-column-count",        // no      http://www.w3.org/TR/css3-multicol/#column-count
        "-moz-column-gap",          // no      http://www.w3.org/TR/css3-multicol/#column-gap
        "-moz-column-rule-color",   // no      http://www.w3.org/TR/css3-multicol/#crc
        "-moz-column-rule-style",   // no      http://www.w3.org/TR/css3-multicol/#column-rule-style
        "-moz-column-rule-width",   // no      http://www.w3.org/TR/css3-multicol/#column-rule-width
        "-moz-column-width",        // no      http://www.w3.org/TR/css3-multicol/#column-width
        "-moz-font-feature-settings",  //
        "-moz-font-language-override", //
        "-moz-hyphens",                //
        "-moz-text-decoration-color",  //
        "-moz-text-decoration-style",  //
        "-moz-text-decoration-line",   //
        "-moz-text-blink",          //
        "-moz-tab-size",            //
      ],
      list: [
        "list-style-image",         // inherit http://www.w3.org/TR/CSS21/propidx.html
        "list-style-position",      // inherit http://www.w3.org/TR/CSS21/propidx.html
        "list-style-type",          // inherit http://www.w3.org/TR/CSS21/propidx.html
        "marker-end",               //
        "marker-mid",               //
        "marker-offset",            //
        "marker-start",             //
      ],
      background: [
        "background-attachment",    // no      http://www.w3.org/TR/css3-background/#background-attachment
        "background-clip",          // no      http://www.w3.org/TR/css3-background/#background-clip
        "background-color",         // no      http://www.w3.org/TR/css3-background/#background-color
        "background-image",         // no      http://www.w3.org/TR/css3-background/#background-image
        "background-origin",        // no      http://www.w3.org/TR/css3-background/#background-origin
        "background-position",      // no      http://www.w3.org/TR/css3-background/#background-position
        "background-repeat",        // no      http://www.w3.org/TR/css3-background/#background-repeat
        "background-size",          // no      http://www.w3.org/TR/css3-background/#background-size
        "-moz-appearance",          //
        "-moz-background-inline-policy", //
      ],
      dims: [
        "width",                    // no      http://www.w3.org/TR/CSS21/propidx.html
        "height",                   // no      http://www.w3.org/TR/CSS21/propidx.html
        "max-width",                // no      http://www.w3.org/TR/CSS21/propidx.html
        "max-height",               // no      http://www.w3.org/TR/CSS21/propidx.html
        "min-width",                // no      http://www.w3.org/TR/CSS21/propidx.html
        "min-height",               // no      http://www.w3.org/TR/CSS21/propidx.html
        "margin-top",               // no      http://www.w3.org/TR/CSS21/propidx.html
        "margin-right",             // no      http://www.w3.org/TR/CSS21/propidx.html
        "margin-bottom",            // no      http://www.w3.org/TR/CSS21/propidx.html
        "margin-left",              // no      http://www.w3.org/TR/CSS21/propidx.html
        "padding-top",              // no      http://www.w3.org/TR/CSS21/propidx.html
        "padding-right",            // no      http://www.w3.org/TR/CSS21/propidx.html
        "padding-bottom",           // no      http://www.w3.org/TR/CSS21/propidx.html
        "padding-left",             // no      http://www.w3.org/TR/CSS21/propidx.html
        "clip",                     // no      http://www.w3.org/TR/CSS21/propidx.html
        "clip-path",                //
        "clip-rule",                //
        "resize",                   // no      http://www.w3.org/TR/css3-ui/#resize
        "stroke-width",             //
        "-moz-box-flex",            //
        "-moz-box-sizing",          // no      http://www.w3.org/TR/css3-ui/#box-sizing
      ],
      pos: [
        "top",                      // no      http://www.w3.org/TR/CSS21/propidx.html
        "right",                    // no      http://www.w3.org/TR/CSS21/propidx.html
        "bottom",                   // no      http://www.w3.org/TR/CSS21/propidx.html
        "left",                     // no      http://www.w3.org/TR/CSS21/propidx.html
        "display",                  // no      http://www.w3.org/TR/CSS21/propidx.html
        "float",                    // no      http://www.w3.org/TR/CSS21/propidx.html
        "clear",                    // no      http://www.w3.org/TR/CSS21/propidx.html
        "position",                 // no      http://www.w3.org/TR/CSS21/propidx.html
        "visibility",               // inherit http://www.w3.org/TR/CSS21/propidx.html
        "overflow",                 //
        "overflow-x",               // no      http://www.w3.org/TR/CSS21/propidx.html
        "overflow-y",               // no      http://www.w3.org/TR/CSS21/propidx.html
        "z-index",                  // no      http://www.w3.org/TR/CSS21/propidx.html
        "dominant-baseline",        //
        "page-break-after",         //
        "page-break-before",        //
        "stroke-dashoffset",        //
        "unicode-bidi",             //
        "-moz-box-align",           //
        "-moz-box-direction",       //
        "-moz-box-ordinal-group",   //
        "-moz-box-orient",          //
        "-moz-box-pack",            //
        "-moz-float-edge",          //
        "-moz-orient",              //
        "-moz-stack-sizing",        //
      ],
      border: [
        "border-top-width",         // no      http://www.w3.org/TR/CSS21/propidx.html
        "border-right-width",       // no      http://www.w3.org/TR/CSS21/propidx.html
        "border-bottom-width",      // no      http://www.w3.org/TR/CSS21/propidx.html
        "border-left-width",        // no      http://www.w3.org/TR/CSS21/propidx.html
        "border-top-color",         // no      http://www.w3.org/TR/CSS21/propidx.html
        "border-right-color",       // no      http://www.w3.org/TR/CSS21/propidx.html
        "border-bottom-color",      // no      http://www.w3.org/TR/CSS21/propidx.html
        "border-left-color",        // no      http://www.w3.org/TR/CSS21/propidx.html
        "border-top-style",         // no      http://www.w3.org/TR/CSS21/propidx.html
        "border-right-style",       // no      http://www.w3.org/TR/CSS21/propidx.html
        "border-bottom-style",      // no      http://www.w3.org/TR/CSS21/propidx.html
        "border-left-style",        // no      http://www.w3.org/TR/CSS21/propidx.html
        "border-collapse",          // no      http://www.w3.org/TR/CSS21/propidx.html
        "border-spacing",           // no      http://www.w3.org/TR/CSS21/propidx.html
        "outline-offset",           // no      http://www.w3.org/TR/CSS21/propidx.html
        "outline-style",            //
        "outline-color",            //
        "outline-width",            //
        "border-top-left-radius",       // no http://www.w3.org/TR/css3-background/#border-radius
        "border-top-right-radius",      // no http://www.w3.org/TR/css3-background/#border-radius
        "border-bottom-right-radius",   // no http://www.w3.org/TR/css3-background/#border-radius
        "border-bottom-left-radius",    // no http://www.w3.org/TR/css3-background/#border-radius
        "-moz-border-bottom-colors",    //
        "-moz-border-image",            //
        "-moz-border-left-colors",      //
        "-moz-border-right-colors",     //
        "-moz-border-top-colors",       //
        "-moz-outline-radius-topleft",      // no http://www.w3.org/TR/CSS2/ui.html#dynamic-outlines ?
        "-moz-outline-radius-topright",     // no http://www.w3.org/TR/CSS2/ui.html#dynamic-outlines ?
        "-moz-outline-radius-bottomright",  // no http://www.w3.org/TR/CSS2/ui.html#dynamic-outlines ?
        "-moz-outline-radius-bottomleft",   // no http://www.w3.org/TR/CSS2/ui.html#dynamic-outlines ?
      ],
      other: [
        "box-shadow",               // no      http://www.w3.org/TR/css3-background/#box-shadow
        "caption-side",             // inherit http://www.w3.org/TR/CSS21/propidx.html
        "content",                  // no      http://www.w3.org/TR/CSS21/propidx.html
        "counter-increment",        // no      http://www.w3.org/TR/CSS21/propidx.html
        "counter-reset",            // no      http://www.w3.org/TR/CSS21/propidx.html
        "cursor",                   // inherit http://www.w3.org/TR/CSS21/propidx.html
        "empty-cells",              // inherit http://www.w3.org/TR/CSS21/propidx.html
        "image-rendering",          // inherit http://www.w3.org/TR/SVG/painting.html#ImageRenderingProperty
        "mask",                     //
        "pointer-events",           // inherit http://www.w3.org/TR/SVG11/interact.html#PointerEventsProperty
        "shape-rendering",          //
        "stroke",                   //
        "stroke-dasharray",         //
        "stroke-linecap",           //
        "stroke-linejoin",          //
        "stroke-miterlimit",        //
        "table-layout",             // no      http://www.w3.org/TR/CSS21/propidx.html
        "-moz-animation-delay",     //
        "-moz-animation-direction", //
        "-moz-animation-duration",  //
        "-moz-animation-fill-mode", //
        "-moz-animation-iteration-count", //
        "-moz-animation-name",            //
        "-moz-animation-play-state",      //
        "-moz-animation-timing-function", //
        "-moz-backface-visibility",       //
        "-moz-binding",                   //
        "-moz-force-broken-image-icon",   //
        "-moz-image-region",        //
        "-moz-perspective",         //
        "-moz-perspective-origin",  //
        "-moz-transform",           // no      http://www.w3.org/TR/css3-2d-transforms/#transform-property
        "-moz-transform-origin",    //
        "-moz-transition-delay",    //
        "-moz-transition-duration", //
        "-moz-transition-property", //
        "-moz-transition-timing-function", //
        "-moz-user-focus",          // inherit http://www.w3.org/TR/2000/WD-css3-userint-20000216#user-focus
        "-moz-user-input",          // inherit http://www.w3.org/TR/2000/WD-css3-userint-20000216#user-input
        "-moz-user-modify",         // inherit http://www.w3.org/TR/2000/WD-css3-userint-20000216#user-modify
        "-moz-user-select",         // no      http://www.w3.org/TR/2000/WD-css3-userint-20000216#user-select
        "-moz-window-shadow",       //
      ],
    };
  },

  /**
   * The CSS groups as displayed by the UI.
   */
  createStyleGroupViews: function CssHtmlTree_createStyleGroupViews()
  {
    if (!CssHtmlTree.propertiesByGroup) {
      let pbg = CssHtmlTree.propertiesByGroup = this._getPropertiesByGroup();

      // Add any supported properties that are not categorized to the "other" group
      let mergedArray = Array.concat(
          pbg.text,
          pbg.list,
          pbg.background,
          pbg.dims,
          pbg.pos,
          pbg.border,
          pbg.other
      );

      // Here we build and cache a list of css properties supported by the browser
      // and store a list to check against. We could use any element but let's
      // use the inspector style panel
      let styles = this.styleWin.contentWindow.getComputedStyle(this.styleDocument.body);
      CssHtmlTree.supportedPropertyLookup = {};
      for (let i = 0, numStyles = styles.length; i < numStyles; i++) {
        let prop = styles.item(i);
        CssHtmlTree.supportedPropertyLookup[prop] = true;

        if (mergedArray.indexOf(prop) == -1) {
          pbg.other.push(prop);
        }
      }

      this.propertiesByGroup = CssHtmlTree.propertiesByGroup;
    }

    let pbg = CssHtmlTree.propertiesByGroup;

    // These group titles are localized by their ID. See the styleinspector.properties file.
    this.styleGroups = [
      new StyleGroupView(this, "Text_Fonts_and_Color", pbg.text),
      new StyleGroupView(this, "Lists", pbg.list),
      new StyleGroupView(this, "Background", pbg.background),
      new StyleGroupView(this, "Dimensions", pbg.dims),
      new StyleGroupView(this, "Positioning_and_Page_Flow", pbg.pos),
      new StyleGroupView(this, "Borders", pbg.border),
      new StyleGroupView(this, "Effects_and_Other", pbg.other),
    ];
  },
};

/**
 * A container to give easy access to style group data from the template engine.
 *
 * @constructor
 * @param {CssHtmlTree} aTree the instance of the CssHtmlTree object that we are
 * working with.
 * @param {string} aId the style group ID.
 * @param {array} aPropertyNames the list of property names associated to this
 * style group view.
 */
function StyleGroupView(aTree, aId, aPropertyNames)
{
  this.tree = aTree;
  this.id = aId;
  this.getRTLAttr = CssHtmlTree.getRTLAttr;
  this.localName = CssHtmlTree.l10n("group." + this.id);

  this.propertyViews = [];
  aPropertyNames.forEach(function(aPropertyName) {
    if (this.isPropertySupported(aPropertyName)) {
      this.propertyViews.push(new PropertyView(this.tree, this, aPropertyName));
    }
  }, this);

  this.populated = false;

  this.templateProperties = this.tree.styleDocument.getElementById("templateProperties");

  // Populated by templater: parent element containing the open attribute
  this.element = null;
  // Destination for templateProperties.
  this.properties = null;
}

StyleGroupView.prototype = {
  /**
   * The click event handler for the title of the style group view.
   */
  click: function StyleGroupView_click()
  {
    // TODO: Animate opening/closing. See bug 587752.
    if (this.element.hasAttribute("open")) {
      this.element.removeAttribute("open");
      return;
    }

    if (!this.populated) {
      CssHtmlTree.processTemplate(this.templateProperties, this.properties, this);
      this.populated = true;
    }

    this.element.setAttribute("open", "");
  },

  /**
   * Close the style group view.
   */
  close: function StyleGroupView_close()
  {
    if (this.element) {
      this.element.removeAttribute("open");
    }
  },

  /**
   * Reset the style group view and its property views.
   *
   * @param {boolean} aClosePanel tells if the style panel is closing or not.
   */
  reset: function StyleGroupView_reset(aClosePanel)
  {
    this.close();
    this.populated = false;
    for (let i = 0, numViews = this.propertyViews.length; i < numViews; i++) {
      this.propertyViews[i].reset();
    }

    if (this.properties) {
      if (aClosePanel) {
        if (this.element) {
          this.element.removeChild(this.properties);
        }

        this.properties = null;
      } else {
        while (this.properties.hasChildNodes()) {
          this.properties.removeChild(this.properties.firstChild);
        }
      }
    }
  },

  /**
   * Check if a CSS property is supported
   *
   * @param {string} aProperty the CSS property to check for
   *
   * @return {boolean} true or false
   */
  isPropertySupported: function(aProperty) {
    return aProperty && aProperty in CssHtmlTree.supportedPropertyLookup;
  },
};

/**
 * A container to give easy access to property data from the template engine.
 *
 * @constructor
 * @param {CssHtmlTree} aTree the CssHtmlTree instance we are working with.
 * @param {StyleGroupView} aGroup the StyleGroupView instance we are working
 * with.
 * @param {string} aName the CSS property name for which this PropertyView
 * instance will render the rules.
 */
function PropertyView(aTree, aGroup, aName)
{
  this.tree = aTree;
  this.group = aGroup;
  this.name = aName;
  this.getRTLAttr = CssHtmlTree.getRTLAttr;

  this.populated = false;
  this.showUnmatched = false;

  this.link = "https://developer.mozilla.org/en/CSS/" + aName;

  this.templateRules = this.tree.styleDocument.getElementById("templateRules");

  // The parent element which contains the open attribute
  this.element = null;
  // Destination for templateRules.
  this.rules = null;

  this.str = {};
}

PropertyView.prototype = {
  /**
   * The click event handler for the property name of the property view. If
   * there are >0 rules then the rules are expanded. If there are 0 rules and
   * >0 unmatched rules then the unmatched rules are expanded instead.
   *
   * @param {Event} aEvent the DOM event
   */
  click: function PropertyView_click(aEvent)
  {
    // Clicking on the property link itself is already handled
    if (aEvent.target.tagName.toLowerCase() == "a") {
      return;
    }

    // TODO: Animate opening/closing. See bug 587752.
    if (this.element.hasAttribute("open")) {
      this.element.removeAttribute("open");
      return;
    }

    if (!this.populated) {
      let matchedRuleCount = this.propertyInfo.matchedRuleCount;

      if (matchedRuleCount == 0 && this.showUnmatchedLink) {
        this.showUnmatchedLinkClick(aEvent);
      } else {
        CssHtmlTree.processTemplate(this.templateRules, this.rules, this);
      }
      this.populated = true;
    }
    this.element.setAttribute("open", "");
  },

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
   * An easy way to access the CssPropertyInfo behind this PropertyView
   */
  get propertyInfo()
  {
    return this.tree.cssLogic.getPropertyInfo(this.name);
  },

  /**
   * Compute the title of the property view. The title includes the number of
   * rules that hold the current property.
   *
   * @param {nsIDOMElement} aElement reference to the DOM element where the rule
   * title needs to be displayed.
   * @return {string} The rule title.
   */
  ruleTitle: function PropertyView_ruleTitle(aElement)
  {
    let result = "";
    let matchedRuleCount = this.propertyInfo.matchedRuleCount;

    if (matchedRuleCount > 0) {
      aElement.classList.add("rule-count");

      let str = CssHtmlTree.l10n("property.numberOfRules");
      result = PluralForm.get(matchedRuleCount, str).replace("#1", matchedRuleCount);
    } else if (this.showUnmatchedLink) {
      aElement.classList.add("rule-unmatched");

      let unmatchedRuleCount = this.propertyInfo.unmatchedRuleCount;
      let str = CssHtmlTree.l10n("property.numberOfUnmatchedRules");
      result = PluralForm.get(unmatchedRuleCount, str).replace("#1", unmatchedRuleCount);
    }
    return result;
  },

  /**
   * Close the property view.
   */
  close: function PropertyView_close()
  {
    if (this.rules && this.element) {
      this.element.removeAttribute("open");
    }
  },

  /**
   * Reset the property view.
   */
  reset: function PropertyView_reset()
  {
    this.close();
    this.populated = false;
    this.showUnmatched = false;
    this.element = false;
  },

  /**
   * Provide access to the SelectorViews that we are currently displaying
   */
  get selectorViews()
  {
    var all = [];

    function convert(aSelectorInfo) {
      all.push(new SelectorView(aSelectorInfo));
    }

    this.propertyInfo.matchedSelectors.forEach(convert);
    if (this.showUnmatched) {
      this.propertyInfo.unmatchedSelectors.forEach(convert);
    }

    return all;
  },

  /**
   * Should we display a 'X unmatched rules' link?
   * @return {boolean} false if we are already showing the unmatched links or
   * if there are none to display, true otherwise.
   */
  get showUnmatchedLink()
  {
    return !this.showUnmatched && this.propertyInfo.unmatchedRuleCount > 0;
  },

  /**
   * The UI has a link to allow the user to display unmatched selectors.
   * This provides localized link text.
   */
  get showUnmatchedLinkText()
  {
    let smur = CssHtmlTree.l10n("rule.showUnmatchedLink");
    let plural = PluralForm.get(this.propertyInfo.unmatchedRuleCount, smur);
    return plural.replace("#1", this.propertyInfo.unmatchedRuleCount);
  },

  /**
   * The action when a user clicks the 'show unmatched' link.
   */
  showUnmatchedLinkClick: function PropertyView_showUnmatchedLinkClick(aEvent)
  {
    this.showUnmatched = true;
    CssHtmlTree.processTemplate(this.templateRules, this.rules, this);
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
   * These statuses are localized inside the styleinspector.properties string bundle.
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
