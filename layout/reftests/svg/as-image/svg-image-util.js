/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
 * @param aViewboxArr         An array of four numbers, representing the
 *                            viewBox attribute, or null for no viewBox.
 * @param aWidth              The width attribute, or null for no width.
 * @param aHeight             The height attribute, or null for no height.
 * @param aAlign              The 'align' component of the
 *                            preserveAspectRatio attribute, or null for none.
 * @param aMeetOrSlice        The 'meetOrSlice' component of the
 *                            preserveAspectRatio attribute, or null for
 *                            none. (If non-null, implies non-null value for
 *                            aAlign.)
 * @param aViewParams         Parameters to use for the view element.
 * @param aFragmentIdentifier The SVG fragment identifier.
 */
function generateSVGDataURI(aViewboxArr, aWidth, aHeight,
                            aAlign, aMeetOrSlice,
                            aViewParams, aFragmentIdentifier) {
  // prefix
  var datauri = "data:image/svg+xml,"
  // Begin the SVG tag
  datauri += "%3Csvg%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%20shape-rendering%3D%22crispEdges%22";

  // Append the custom chunk from our params
  // If we're working with views, the align customisation is applied there instead
  datauri += generateSVGAttrsForParams(aViewboxArr, aWidth, aHeight,
                                       aViewParams ? null : aAlign,
                                       aMeetOrSlice);

  // Add 'font-size' just in case the client wants to use ems
  datauri += "%20font-size%3D%22" + "10px" + "%22";

  // Put closing right bracket on SVG tag
  datauri += "%3E";

  if (aViewParams) {
    // Give the view the id of the fragment identifier
    datauri += "%3Cview%20id%3D%22" + aFragmentIdentifier + "%22";

    // Append the custom chunk from our view params
    datauri += generateSVGAttrsForParams(aViewParams.viewBox, null, null,
                                         aAlign, aViewParams.meetOrSlice);

    datauri += "%2F%3E";
  }

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
        str += aViewboxArr[i];
        if (i != aViewboxArr.length - 1) {
          str += "%20";
        }
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
  // These are width & height vals that will be used for the *host node*.
  // (i.e. the <img> or <embed> node -- not the <svg> node)
  var hostNodeWidthVals  = [ null, HOST_NODE_WIDTH  ];
  var hostNodeHeightVals = [ null, HOST_NODE_HEIGHT ];

  for (var i = 0; i < hostNodeWidthVals.length; i++) {
    var hostNodeWidth = hostNodeWidthVals[i];
    for (var j = 0; j < hostNodeHeightVals.length; j++) {
      var hostNodeHeight = hostNodeHeightVals[j];
      appendSVGSubArrayWithParams(aSVGParams, aHostNodeTagName,
                                  hostNodeWidth, hostNodeHeight);
    }
  }
}

// Helper function for above, for a fixed [host-node-width][host-node-height]
function appendSVGSubArrayWithParams(aSVGParams, aHostNodeTagName,
                                     aHostNodeWidth, aHostNodeHeight) {
  var rootNode = document.getElementsByTagName("body")[0];
  for (var k = 0; k < ALIGN_VALS.length; k++) {
    var alignVal = ALIGN_VALS[k];
    if (!aSVGParams.meetOrSlice) {
      alignVal = "none";
    }

    // Generate the Data URI
    var uri = generateSVGDataURI(aSVGParams.viewBox,
                                 aSVGParams.width, aSVGParams.height,
                                 alignVal,
                                 aSVGParams.meetOrSlice,
                                 aSVGParams.view,
                                 aSVGParams.fragmentIdentifier);

    if (aSVGParams.fragmentIdentifier) {
      uri += "#" + aSVGParams.fragmentIdentifier;
    }

    // Generate & append the host node element
    var hostNode = generateHostNode(aHostNodeTagName, uri,
                                    aHostNodeWidth, aHostNodeHeight);
    rootNode.appendChild(hostNode);

    // Cosmetic: Add a newline when we get halfway through the ALIGN_VALS
    // and then again when we reach the end
    if (k + 1 == ALIGN_VALS.length / 2 ||
        k + 1 == ALIGN_VALS.length) {
      rootNode.appendChild(document.createElement("br"));
    }
  }
}
