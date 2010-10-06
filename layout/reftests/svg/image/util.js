/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is SVG Testing Code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Holbert <dholbert@mozilla.com> (original author)
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

// The possible values of the "align" component of preserveAspectRatio.
const ALIGN_VALS = ["none",
                    "xMinYMin", "xMinYMid", "xMinYMax",
                    "xMidYMin", "xMidYMid", "xMidYMax",
                    "xMaxYMin", "xMaxYMid", "xMaxYMax"];

// The possible values of the "meetOrSlice" component of preserveAspectRatio.
const MEETORSLICE_VALS = [ "meet", "slice" ];

const SVGNS   = "http://www.w3.org/2000/svg";
const XLINKNS = "http://www.w3.org/1999/xlink";

// This is the separation between the x & y values of each <image> in a
// generated grid.
const IMAGE_OFFSET = 50;

function generateBorderRect(aX, aY, aWidth, aHeight) {
  var rect = document.createElementNS(SVGNS, "rect");
  rect.setAttribute("x", aX);
  rect.setAttribute("y", aY);
  rect.setAttribute("width", aWidth);
  rect.setAttribute("height", aHeight);
  rect.setAttribute("fill", "none");
  rect.setAttribute("stroke", "black");
  rect.setAttribute("stroke-width", "2");
  rect.setAttribute("stroke-dasharray", "3 2");
  return rect;
}

// Returns an SVG <image> element with the given xlink:href, width, height,
// and preserveAspectRatio=[aAlign aMeetOrSlice] attributes
function generateImageElementForParams(aX, aY, aWidth, aHeight,
                                       aHref, aAlign, aMeetOrSlice) {
  var image = document.createElementNS(SVGNS, "image");
  image.setAttribute("x", aX);
  image.setAttribute("y", aY);
  image.setAttribute("width", aWidth);
  image.setAttribute("height", aHeight);
  image.setAttributeNS(XLINKNS, "href", aHref);
  image.setAttribute("preserveAspectRatio", aAlign + " " + aMeetOrSlice);
  return image;
}

// Returns a <g> element filled with a grid of <image> elements which each
// have the specified aWidth & aHeight and which use all possible values of
// preserveAspectRatio.
function generateImageGrid(aHref, aWidth, aHeight) {
  var grid = document.createElementNS(SVGNS, "g");
  var y = 0;
  var x = 0;
  for (var i = 0; i < ALIGN_VALS.length; i++) {
    // Jump to next line of grid, for every other "i" value.
    // (every fourth entry)
    if (i && i % 2 == 0) {
      y += IMAGE_OFFSET;
      x = 0;
    }
    var alignVal = ALIGN_VALS[i];
    for (var j = 0; j < MEETORSLICE_VALS.length; j++) {
      var meetorsliceVal = MEETORSLICE_VALS[j];
      var border = generateBorderRect(x, y, aWidth, aHeight);
      var image  = generateImageElementForParams(x, y, aWidth, aHeight,
                                                 aHref, alignVal,
                                                 meetorsliceVal);
      grid.appendChild(border);
      grid.appendChild(image);
      x += IMAGE_OFFSET;
    }
  }
  return grid;
}

// Returns an SVG <symbol> element that...
//  (a) has the given ID
//  (b) contains only a <use> element to the given URI
//  (c) has a hardcoded viewBox="0 0 10 10" attribute
//  (d) has the given preserveAspectRatio=[aAlign aMeetOrSlice] attribute
function generateSymbolElementForParams(aSymbolID, aHref,
                                        aAlign, aMeetOrSlice) {
  var use = document.createElementNS(SVGNS, "use");
  use.setAttributeNS(XLINKNS, "href", aHref);

  var symbol = document.createElementNS(SVGNS, "symbol");
  symbol.setAttribute("id", aSymbolID);
  symbol.setAttribute("viewBox", "0 0 10 10");
  symbol.setAttribute("preserveAspectRatio", aAlign + " " + aMeetOrSlice);

  symbol.appendChild(use);
  return symbol;
}

function generateUseElementForParams(aTargetURI, aX, aY, aWidth, aHeight) {
  var use = document.createElementNS(SVGNS, "use");
  use.setAttributeNS(XLINKNS, "href", aTargetURI);
  use.setAttribute("x", aX);
  use.setAttribute("y", aY);
  use.setAttribute("width", aWidth);
  use.setAttribute("height", aHeight);
  return use;
}
// Returns a <g> element filled with a grid of <use> elements which each
// have the specified aWidth & aHeight and which reference <symbol> elements
// with all possible values of preserveAspectRatio.  Each <symbol> contains
// a <use> that links to the given URI, aHref.
function generateSymbolGrid(aHref, aWidth, aHeight) {
  var grid = document.createElementNS(SVGNS, "g");
  var y = 0;
  var x = 0;
  for (var i = 0; i < ALIGN_VALS.length; i++) {
    // Jump to next line of grid, for every other "i" value.
    // (every fourth entry)
    if (i && i % 2 == 0) {
      y += IMAGE_OFFSET;
      x = 0;
    }
    var alignVal = ALIGN_VALS[i];
    for (var j = 0; j < MEETORSLICE_VALS.length; j++) {
      var meetorsliceVal = MEETORSLICE_VALS[j];
      var border = generateBorderRect(x, y, aWidth, aHeight);

      var symbolID = "symbol_" + alignVal + "_" + meetorsliceVal;
      var symbol = generateSymbolElementForParams(symbolID, aHref,
                                                  alignVal, meetorsliceVal);
      var use = generateUseElementForParams("#" + symbolID,
                                            x, y, aWidth, aHeight);
      grid.appendChild(symbol); // This isn't painted
      grid.appendChild(border);
      grid.appendChild(use);
      x += IMAGE_OFFSET;
    }
  }
  return grid;
}
