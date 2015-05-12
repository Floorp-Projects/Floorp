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
      "-webkit-transform-origin":  "transform-origin",
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

  // See documentation in nsICSSUnprefixingService.idl
  generateUnprefixedGradientValue: function(aPrefixedFuncName,
                                            aPrefixedFuncBody,
                                            aUnprefixedFuncName, /*[out]*/
                                            aUnprefixedFuncBody /*[out]*/) {
    var unprefixedFuncName, newValue;
    if (aPrefixedFuncName == "-webkit-gradient") {
      // Create expression for oldGradientParser:
      var parts = this.oldGradientParser(aPrefixedFuncBody);
      var type = parts[0].name;
      newValue = this.standardizeOldGradientArgs(type, parts.slice(1));
      unprefixedFuncName = type + "-gradient";
    }else{ // we're dealing with more modern syntax - should be somewhat easier, at least for linear gradients.
        // Fix three things: remove -webkit-, add 'to ' before reversed top/bottom keywords (linear) or 'at ' before position keywords (radial), recalculate deg-values
        // -webkit-linear-gradient( [ [ <angle> | [top | bottom] || [left | right] ],]? <color-stop>[, <color-stop>]+);
        if (aPrefixedFuncName != "-webkit-linear-gradient" &&
            aPrefixedFuncName != "-webkit-radial-gradient") {
          // Unrecognized prefixed gradient type
          return false;
        }
        unprefixedFuncName = aPrefixedFuncName.replace(/-webkit-/, '');

        // Keywords top, bottom, left, right: can be stand-alone or combined pairwise but in any order ('top left' or 'left top')
        // These give the starting edge or corner in the -webkit syntax. The standardised equivalent is 'to ' plus opposite values for linear gradients, 'at ' plus same values for radial gradients
        if(unprefixedFuncName.indexOf('linear') > -1){
            newValue = aPrefixedFuncBody.replace(/(top|bottom|left|right)+\s*(top|bottom|left|right)*/, function(str){
                var words = str.split(/\s+/);
                for(var i=0; i<words.length; i++){
                    switch(words[i].toLowerCase()){
                        case 'top':
                            words[i] = 'bottom';
                            break;
                        case 'bottom':
                            words[i] = 'top';
                            break;
                        case 'left':
                            words[i] = 'right';
                            break;
                        case 'right':
                            words[i] = 'left';
                    }
                }
                str = words.join(' ');
                return ( 'to ' + str);
            });
        }else{
            newValue = aPrefixedFuncBody.replace(/(top|bottom|left|right)+\s/, 'at $1 ');
        }

        newValue = newValue.replace(/\d+deg/, function (val) {
             return (360 - (parseInt(val)-90))+'deg';
         });

    }
    aUnprefixedFuncName.value = unprefixedFuncName;
    aUnprefixedFuncBody.value = newValue;
    return true;
  },

  // Helpers for generateUnprefixedGradientValue():
  // ----------------------------------------------
  oldGradientParser : function(str){
    /** This method takes a legacy -webkit-gradient() method call and parses it
        to pull out the values, function names and their arguments.
        It returns something like [{name:'-webkit-gradient',args:[{name:'linear'}, {name:'top left'} ... ]}]
    */
    var objs = [{}], path=[], current, word='', separator_chars = [',', '(', ')'];
    current = objs[0], path[0] = objs;
    //str = str.replace(/\s*\(/g, '('); // sorry, ws in front of ( would make parsing a lot harder
    for(var i = 0; i < str.length; i++){
        if(separator_chars.indexOf(str[i]) === -1){
            word += str[i];
        }else{ // now we have a "separator" - presumably we've also got a "word" or value
            current.name = word.trim();
            //GM_log(word+' '+path.length+' '+str[i])
            word = '';
            if(str[i] === '('){ // we assume the 'word' is a function, for example -webkit-gradient() or rgb(), so we create a place to record the arguments
                if(!('args' in current)){
                   current.args = [];
                }
                current.args.push({});
                path.push(current.args);
                current = current.args[current.args.length - 1];
                path.push(current);
            }else if(str[i] === ')'){ // function is ended, no more arguments - go back to appending details to the previous branch of the tree
                current = path.pop(); // drop 'current'
                current = path.pop(); // drop 'args' reference
            }else{
                path.pop(); // remove 'current' object from path, we have no arguments to add
                var current_parent = path[path.length - 1] || objs; // last object on current path refers to array that contained the previous "current"
                current_parent.push({}); // we need a new object to hold this "word" or value
                current = current_parent[current_parent.length - 1]; // that object is now the 'current'
                path.push(current);
//GM_log(path.length)
            }
        }
    }

    return objs;
  },

  /* Given an array of args for "-webkit-gradient(...)" returned by
   * oldGradientParser(), this function constructs a string representing the
   * equivalent arguments for a standard "linear-gradient(...)" or
   * "radial-gradient(...)" expression.
   *
   * @param type  Either 'linear' or 'radial'.
   * @param args  An array of args for a "-webkit-gradient(...)" expression,
   *              provided by oldGradientParser() (not including gradient type).
   */
  standardizeOldGradientArgs : function(type, args){
    var stdArgStr = "";
    var stops = [];
    if(/^linear/.test(type)){
        // linear gradient, args 1 and 2 tend to be start/end keywords
        var points = [].concat(args[0].name.split(/\s+/), args[1].name.split(/\s+/)); // example: [left, top, right, top]
        // Old webkit syntax "uses a two-point syntax that lets you explicitly state where a linear gradient starts and ends"
        // if start/end keywords are percentages, let's massage the values a little more..
        var rxPercTest = /\d+\%/;
        if(rxPercTest.test(points[0]) || points[0] == 0){
            var startX = parseInt(points[0]), startY = parseInt(points[1]), endX = parseInt(points[2]), endY = parseInt(points[3]);
            stdArgStr +=  ((Math.atan2(endY- startY, endX - startX)) * (180 / Math.PI)+90) + 'deg';
        }else{
            if(points[1] === points[3]){ // both 'top' or 'bottom, this linear gradient goes left-right
                stdArgStr += 'to ' + points[2];
            }else if(points[0] === points[2]){ // both 'left' or 'right', this linear gradient goes top-bottom
                stdArgStr += 'to ' + points[3];
            }else if(points[1] === 'top'){ // diagonal gradient - from top left to opposite corner is 135deg
                stdArgStr += '135deg';
            }else{
                stdArgStr += '45deg';
            }
        }

    }else if(/^radial/i.test(type)){ // oooh, radial gradients..
        stdArgStr += 'circle ' + args[3].name.replace(/(\d+)$/, '$1px') + ' at ' + args[0].name.replace(/(\d+) /, '$1px ').replace(/(\d+)$/, '$1px');
    }

    var toColor;
    for(var j = type === 'linear' ? 2 : 4; j < args.length; j++){
        var position, color, colorIndex;
        if(args[j].name === 'color-stop'){
            position = args[j].args[0].name;
            colorIndex = 1;
        }else if (args[j].name === 'to') {
            position = '100%';
            colorIndex = 0;
        }else if (args[j].name === 'from') {
            position = '0%';
            colorIndex = 0;
        };
        if (position.indexOf('%') === -1) { // original Safari syntax had 0.5 equivalent to 50%
            position = (parseFloat(position) * 100) +'%';
        };
        color = args[j].args[colorIndex].name;
        if (args[j].args[colorIndex].args) { // the color is itself a function call, like rgb()
            color += '(' + this.colorValue(args[j].args[colorIndex].args) + ')';
        };
        if (args[j].name === 'from'){
            stops.unshift(color + ' ' + position);
        }else if(args[j].name === 'to'){
            toColor = color;
        }else{
            stops.push(color + ' ' + position);
        }
    }

    // translating values to right syntax
    for(var j = 0; j < stops.length; j++){
        stdArgStr += ', ' + stops[j];
    }
    if(toColor){
        stdArgStr += ', ' + toColor + ' 100%';
    }
    return stdArgStr;
  },

  colorValue: function(obj){
    var ar = [];
    for (var i = 0; i < obj.length; i++) {
        ar.push(obj[i].name);
    };
    return ar.join(', ');
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([CSSUnprefixingService]);
