/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
//
// The final "aBonusPARVal" argument (if specified) is used as the
// preserveAspectRatio value on a bonus <image> element, added at the end.
function generateImageGrid(aHref, aWidth, aHeight, aBonusPARVal) {
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

  if (aBonusPARVal) {
    // Add one final entry with "bonus" pAR value.
    y += IMAGE_OFFSET;
    x = 0;
    var border = generateBorderRect(x, y, aWidth, aHeight);
    var image  = generateImageElementForParams(x, y, aWidth, aHeight,
                                               aHref, aBonusPARVal, "");
    grid.appendChild(border);
    grid.appendChild(image);
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
//
// The final "aBonusPARVal" argument (if specified) is used as the
// preserveAspectRatio value on a bonus <symbol> element, added at the end.
function generateSymbolGrid(aHref, aWidth, aHeight, aBonusPARVal) {
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

  if (aBonusPARVal) {
    // Add one final entry with "bonus" pAR value.
    y += IMAGE_OFFSET;
    x = 0;
    var border = generateBorderRect(x, y, aWidth, aHeight);
    var symbolID = "symbol_Bonus";
    var symbol = generateSymbolElementForParams(symbolID, aHref,
                                                aBonusPARVal, "");
    var use = generateUseElementForParams("#" + symbolID,
                                          x, y, aWidth, aHeight);
    grid.appendChild(symbol); // This isn't painted
    grid.appendChild(border);
    grid.appendChild(use);
  }

  return grid;
}
