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

// Standard values to use for <img>/<embed> height & width, if requested.
var HOST_NODE_HEIGHT = "20";
var HOST_NODE_WIDTH =  "30";

// All the possible values of "align"
const ALIGN_VALS = ["none",
                    "xMinYMin", "xMinYMid", "xMinYMax",
                    "xMidYMin", "xMidYMid", "xMidYMax",
                    "xMaxYMin", "xMaxYMid", "xMaxYMax"];

// All the possible values of "meetOrSlice"
const MEETORSLICE_VALS = [ "meet", "slice" ];

/**
 * Generates full data URI for an SVG document, with the given parameters
 * on the SVG element.
 *
 * @param aViewboxArr   An array of four numbers, representing the viewBox
 *                      attribute, or null for no viewBox.
 * @param aWidth        The width attribute, or null for no width.
 * @param aHeight       The height attribute, or null for no height.
 * @param aAlign        The 'align' component of the preserveAspectRatio
 *                      attribute, or null for none.
 * @param aMeetOrSlice  The 'meetOrSlice' component of the
 *                      preserveAspectRatio attribute, or null for
 *                      none. (If non-null, implies non-null value for
 *                      aAlign.)
 */
function generateSVGDataURI(aViewboxArr, aWidth, aHeight,
                            aAlign, aMeetOrSlice) {
  // prefix
  var datauri = "data:image/svg+xml,"
  // Begin the SVG tag
  datauri += "%3Csvg%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22";

  // Append the custom chunk from our params
  datauri += generateSVGAttrsForParams(aViewboxArr, aWidth, aHeight,
                                       aAlign, aMeetOrSlice);

  // Put closing leftbracket on SVG tag
  datauri += "%3E";

  // Add the rest of the SVG document
  datauri += "%3Crect%20x%3D%221%22%20y%3D%221%22%20height%3D%2218%22%20width%3D%2218%22%20stroke-width%3D%222%22%20stroke%3D%22black%22%20fill%3D%22yellow%22%2F%3E%3Ccircle%20cx%3D%2210%22%20cy%3D%2210%22%20r%3D%228%22%20style%3D%22fill%3A%20blue%22%2F%3E%3C%2Fsvg%3E";

  return datauri;
}

// Generates just the chunk of a data URI that's relevant to
// the given params.
function generateSVGAttrsForParams(aViewboxArr, aWidth, aHeight,
                                   aAlign, aMeetOrSlice) {
  var str = "";
  if (aViewboxArr) {
    str += "%20viewBox%3D%22";
    for (var i in aViewboxArr) {
        var curVal = aViewboxArr[i];
        str += curVal + "%20";
    }
    str += "%22";
  }
  if (aWidth) {
    str += "%20width%3D%22"  + aWidth  + "%22";
  }
  if (aHeight) {
    str += "%20height%3D%22" + aHeight + "%22";
  }
  if (aAlign) {
    str += "%20preserveAspectRatio%3D%22" + aAlign;
    if (aMeetOrSlice) {
      str += "%20" + aMeetOrSlice;
    }
    str += "%22";
  }
  return str;
}

// Returns a newly-generated element with the given tagname, the given URI
// for its |src| attribute, and the given width & height values.
function generateHostNode(aHostNodeTagName, aUri,
                          aHostNodeWidth, aHostNodeHeight) {
  var elem = document.createElement(aHostNodeTagName);
  elem.setAttribute("src", aUri);

  if (aHostNodeWidth) {
    elem.setAttribute("width", aHostNodeWidth);
  }
  if (aHostNodeHeight) {
    elem.setAttribute("height", aHostNodeHeight);
  }

  return elem;
}

// THIS IS THE CHIEF HELPER FUNCTION TO BE CALLED BY CLIENTS
function appendSVGArrayWithParams(aSVGParams, aHostNodeTagName) {
  var rootNode = document.getElementsByTagName("body")[0];

  // These are width & height vals that will be used for the *host node*.
  // (i.e. the <img> or <embed> node -- not the <svg> node)
  var hostNodeWidthVals  = [ null, HOST_NODE_WIDTH  ];
  var hostNodeHeightVals = [ null, HOST_NODE_HEIGHT ];

  for (var i = 0; i < hostNodeWidthVals.length; i++) {
    var hostWidth = hostNodeWidthVals[i];
    for (var j = 0; j < hostNodeHeightVals.length; j++) {
      var hostHeight = hostNodeHeightVals[j];
      for (var k = 0; k < ALIGN_VALS.length; k++) {
        var alignVal = ALIGN_VALS[k];

        // Generate the Data URI
        var uri = generateSVGDataURI(aSVGParams.viewBox,
                                     aSVGParams.width, aSVGParams.height,
                                     alignVal,
                                     aSVGParams.meetOrSlice);

        // Generate & append the host node element
        var hostNode = generateHostNode(aHostNodeTagName, uri,
                                        hostWidth, hostHeight);
        rootNode.appendChild(hostNode);

        // Cosmetic: Add a newline when we get halfway through the ALIGN_VALS
        // and then again when we reach the end
        if (k + 1 == ALIGN_VALS.length / 2 ||
            k + 1 == ALIGN_VALS.length) {
          rootNode.appendChild(document.createElement("br"));
        }
      }
    }
  }
}
