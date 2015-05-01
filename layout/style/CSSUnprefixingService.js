/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- /
/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implementation of a service that converts certain vendor-prefixed CSS
   properties to their unprefixed equivalents, for sites on a whitelist. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function CSSUnprefixingService() {
}

CSSUnprefixingService.prototype = {
  // Boilerplate:
  classID:        Components.ID("{f0729490-e15c-4a2f-a3fb-99e1cc946b42}"),
  _xpcom_factory: XPCOMUtils.generateSingletonFactory(CSSUnprefixingService),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICSSUnprefixingService]),

  // See documentation in nsICSSUnprefixingService.idl
  generateUnprefixedDeclaration: function(aPropName, aRightHalfOfDecl,
                                          aUnprefixedDecl /*out*/) {

    // Convert our input strings to lower-case, for easier string-matching.
    // (NOTE: If we ever need to add support for unprefixing properties that
    // have case-sensitive parts, then we should do these toLowerCase()
    // conversions in a more targeted way, to avoid breaking those properties.)
    aPropName = aPropName.toLowerCase();
    aRightHalfOfDecl = aRightHalfOfDecl.toLowerCase();

    // We have several groups of supported properties:
    // FIRST GROUP: Properties that can just be handled as aliases:
    // ============================================================
    const propertiesThatAreJustAliases = {
      "-webkit-background-size":   "background-size",
      "-webkit-box-flex":          "flex-grow",
      "-webkit-box-ordinal-group": "order",
      "-webkit-box-sizing":        "box-sizing",
      "-webkit-transform":         "transform",
    };

    let unprefixedPropName = propertiesThatAreJustAliases[aPropName];
    if (unprefixedPropName !== undefined) {
      aUnprefixedDecl.value = unprefixedPropName + ":" + aRightHalfOfDecl;
      return true;
    }

    // SECOND GROUP: Properties that take a single keyword, where the
    // unprefixed version takes a different (but analogous) set of keywords:
    // =====================================================================
    const propertiesThatNeedKeywordMapping = {
      "-webkit-box-align" : {
        unprefixedPropName : "align-items",
        valueMap : {
          "start"    : "flex-start",
          "center"   : "center",
          "end"      : "flex-end",
          "baseline" : "baseline",
          "stretch"  : "stretch"
        }
      },
      "-webkit-box-orient" : {
        unprefixedPropName : "flex-direction",
        valueMap : {
          "horizontal"  : "row",
          "inline-axis" : "row",
          "vertical"    : "column",
          "block-axis"  : "column"
        }
      },
      "-webkit-box-pack" : {
        unprefixedPropName : "justify-content",
        valueMap : {
          "start"    : "flex-start",
          "center"   : "center",
          "end"      : "flex-end",
          "justify"  : "space-between"
        }
      },
    };

    let propInfo = propertiesThatNeedKeywordMapping[aPropName];
    if (typeof(propInfo) != "undefined") {
      // Regexp for parsing the right half of a declaration, for keyword-valued
      // properties. Divides the right half of the declaration into:
      //  1) any leading whitespace
      //  2) the property value (one or more alphabetical character or hyphen)
      //  3) anything after that (e.g. "!important", ";")
      // Then we can look up the appropriate unprefixed-property value for the
      // value (part 2), and splice that together with the other parts and with
      // the unprefixed property-name to make the final declaration.
      const keywordValuedPropertyRegexp = /^(\s*)([a-z\-]+)(.*)/;
      let parts = keywordValuedPropertyRegexp.exec(aRightHalfOfDecl);
      if (!parts) {
        // Failed to parse a keyword out of aRightHalfOfDecl. (It probably has
        // no alphabetical characters.)
        return false;
      }

      let mappedKeyword = propInfo.valueMap[parts[2]];
      if (mappedKeyword === undefined) {
        // We found a keyword in aRightHalfOfDecl, but we don't have a mapping
        // to an equivalent keyword for the unprefixed version of the property.
        return false;
      }

      aUnprefixedDecl.value = propInfo.unprefixedPropName + ":" +
        parts[1] + // any leading whitespace
        mappedKeyword +
        parts[3]; // any trailing text (e.g. !important, semicolon, etc)

      return true;
    }

    // THIRD GROUP: Properties that may need arbitrary string-replacement:
    // ===================================================================
    const propertiesThatNeedStringReplacement = {
      // "-webkit-transition" takes a multi-part value. If "-webkit-transform"
      // appears as part of that value, replace it w/ "transform".
      // And regardless, we unprefix the "-webkit-transition" property-name.
      // (We could handle other prefixed properties in addition to 'transform'
      // here, but in practice "-webkit-transform" is the main one that's
      // likely to be transitioned & that we're concerned about supporting.)
      "-webkit-transition": {
        unprefixedPropName : "transition",
        stringMap : {
          "-webkit-transform" : "transform",
        }
      },
    };

    propInfo = propertiesThatNeedStringReplacement[aPropName];
    if (typeof(propInfo) != "undefined") {
      let newRightHalf = aRightHalfOfDecl;
      for (let strToReplace in propInfo.stringMap) {
        let replacement = propInfo.stringMap[strToReplace];
        newRightHalf = newRightHalf.split(strToReplace).join(replacement);
      }
      aUnprefixedDecl.value = propInfo.unprefixedPropName + ":" + newRightHalf;

      return true;
    }

    // No known mapping for property aPropName.
    return false;
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([CSSUnprefixingService]);
