function toFixed(num, fixed) {
    fixed = fixed || 0;
    fixed = Math.pow(10, fixed);
    return Math.floor(num * fixed) / fixed;
}
function createElement(name, props) {
  var el = document.createElement(name);

  for (var key in props) {
    if (key === "style") {
      for (var styleName in props.style) {
        el.style[styleName] = props.style[styleName];
      }
    } else {
      el[key] = props[key];
    }
  }

  return el;
}

function parseDisplayList(lines) {
  var root = {
    line: "DisplayListRoot 0",
    name: "DisplayListRoot",
    address: "0x0",
    frame: "Root",
    children: [],
  };

  var objectAtIndentation = {
    "-1": root,
  };

  for (var i = 0; i < lines.length; i++) {
    var line = lines[i];

    var layerObject = {
      line: line,
      children: [],
    }
    if (!root) {
      root = layerObject;
    }

    var matches = line.match("(\\s*)(\\w+)\\sp=(\\w+)\\sf=(.*?)\\((.*?)\\)\\s(z=(\\w+)\\s)?(.*?)?( layer=(\\w+))?$");
    if (!matches) {
      dump("Failed to match: " + line + "\n");
      continue;
    }

    var indentation = Math.floor(matches[1].length / 2);
    objectAtIndentation[indentation] = layerObject;
    var parent = objectAtIndentation[indentation - 1];
    if (parent) {
      parent.children.push(layerObject);
    }

    layerObject.name = matches[2];
    layerObject.address = matches[3]; // Use 0x prefix to be consistent with layer dump
    layerObject.frame = matches[4];
    layerObject.contentDescriptor = matches[5];
    layerObject.z = matches[7];
    var rest = matches[8];
    if (matches[10]) { // WrapList don't provide a layer
      layerObject.layer = matches[10];
    }
    layerObject.rest = rest;

    // the content node name doesn't have a prefix, this makes the parsing easier
    rest = "content" + rest;

    var fields = {};
    var nesting = 0;
    var startIndex;
    var lastSpace = -1;
    var lastFieldStart = -1;
    for (var j = 0; j < rest.length; j++) {
      if (rest.charAt(j) == '(') {
        nesting++;
        if (nesting == 1) {
          startIndex = j;
        }
      } else if (rest.charAt(j) == ')') {
        nesting--;
        if (nesting == 0) {
          var name = rest.substring(lastSpace + 1, startIndex);
          var value = rest.substring(startIndex + 1, j);

          var rectMatches = value.match("^(.*?),(.*?),(.*?),(.*?)$")
          if (rectMatches) {
            layerObject[name] = [
              parseFloat(rectMatches[1]),
              parseFloat(rectMatches[2]),
              parseFloat(rectMatches[3]),
              parseFloat(rectMatches[4]),
            ];
          } else {
            layerObject[name] = value;
          }
        }
      } else if (nesting == 0 && rest.charAt(j) == ' ') {
        lastSpace = j;
      }
    }
    //dump("FIELDS: " + JSON.stringify(fields) + "\n");
  }
  return root;
}

function trim(s){ 
  return ( s || '' ).replace( /^\s+|\s+$/g, '' ); 
}

function getDataURI(str) {
  if (str.indexOf("data:image/png;base64,") == 0) {
    return str;
  }

  var matches = str.match("data:image/lz4bgra;base64,([0-9]+),([0-9]+),([0-9]+),(.*)");
  if (!matches)
    return null;

  var canvas = document.createElement("canvas");
  var w = parseInt(matches[1]);
  var stride = parseInt(matches[2]);
  var h = parseInt(matches[3]);
  canvas.width = w;
  canvas.height = h;

  // TODO handle stride

  var binary_string = window.atob(matches[4]);
  var len = binary_string.length;
  var bytes = new Uint8Array(len);
  var decoded = new Uint8Array(stride * h);
  for (var i = 0; i < len; i++) {
    var ascii = binary_string.charCodeAt(i);
    bytes[i] = ascii;
  }

  var ctxt = canvas.getContext("2d");
  var out = ctxt.createImageData(w, h);
  buffer = LZ4_uncompressChunk(bytes, decoded);

  for (var x = 0; x < w; x++) {
    for (var y = 0; y < h; y++) {
      out.data[4 * x + 4 * y * w + 0] = decoded[4 * x + y * stride + 2];
      out.data[4 * x + 4 * y * w + 1] = decoded[4 * x + y * stride + 1];
      out.data[4 * x + 4 * y * w + 2] = decoded[4 * x + y * stride + 0];
      out.data[4 * x + 4 * y * w + 3] = decoded[4 * x + y * stride + 3];
    }
  }

  ctxt.putImageData(out, 0, 0);
  return canvas.toDataURL();
}

function parseLayers(layersDumpLines) {
  function parseMatrix2x3(str) {
    str = trim(str);

    // Something like '[ 1 0; 0 1; 0 158; ]'
    var matches = str.match("^\\[ (.*?) (.*?); (.*?) (.*?); (.*?) (.*?); \\]$");
    if (!matches) {
      return null;
    }

    var matrix = [
      [parseFloat(matches[1]), parseFloat(matches[2])],
      [parseFloat(matches[3]), parseFloat(matches[4])],
      [parseFloat(matches[5]), parseFloat(matches[6])],
    ];

    return matrix;
  }
  function parseColor(str) {
    str = trim(str);

    // Something like 'rgba(0, 0, 0, 0)'
    var colorMatches = str.match("^rgba\\((.*), (.*), (.*), (.*)\\)$");
    if (!colorMatches) {
      return null;
    }

    var color = {
      r: colorMatches[1],
      g: colorMatches[2],
      b: colorMatches[3],
      a: colorMatches[4],
    };
    return color;
  }
  function parseFloat_cleo(str) {
    str = trim(str);

    // Something like 2.000
    if (parseFloat(str) == str) {
      return parseFloat(str);
    }

    return null;
  }
  function parseRect2D(str) {
    str = trim(str);

    // Something like '(x=0, y=0, w=2842, h=158)'
    var rectMatches = str.match("^\\(x=(.*?), y=(.*?), w=(.*?), h=(.*?)\\)$");
    if (!rectMatches) {
      return null;
    }

    var rect = [
      parseFloat(rectMatches[1]), parseFloat(rectMatches[2]),
      parseFloat(rectMatches[3]), parseFloat(rectMatches[4]),
    ];
    return rect;
  }
  function parseRegion(str) {
    str = trim(str);

    // Something like '< (x=0, y=0, w=2842, h=158); (x=0, y=1718, w=2842, h=500); >'
    if (str.charAt(0) != '<' || str.charAt(str.length - 1) != '>') {
      return null;
    }

    var region = [];
    str = trim(str.substring(1, str.length - 1));
    while (str != "") {
      var rectMatches = str.match("^\\(x=(.*?), y=(.*?), w=(.*?), h=(.*?)\\);(.*)$");
      if (!rectMatches) {
        return null;
      }

      var rect = [
        parseFloat(rectMatches[1]), parseFloat(rectMatches[2]),
        parseFloat(rectMatches[3]), parseFloat(rectMatches[4]),
      ];
      str = trim(rectMatches[5]);
      region.push(rect);
    }
    return region;
  }

  var LAYERS_LINE_REGEX = "(\\s*)(\\w+)\\s\\((\\w+)\\)(.*)";

  var root;
  var objectAtIndentation = [];
  for (var i = 0; i < layersDumpLines.length; i++) {
    // Something like 'ThebesLayerComposite (0x12104cc00) [shadow-visible=< (x=0, y=0, w=1920, h=158); >] [visible=< (x=0, y=0, w=1920, h=158); >] [opaqueContent] [valid=< (x=0, y=0, w=1920, h=2218); >]'
    var line = layersDumpLines[i].name || layersDumpLines[i];

    var tileMatches = line.match("(\\s*)Tile \\(x=(.*), y=(.*)\\): (.*)");
    if (tileMatches) {
      var indentation = Math.floor(matches[1].length / 2);
      var x = tileMatches[2];
      var y = tileMatches[3];
      var dataUri = tileMatches[4];
      var parent = objectAtIndentation[indentation - 1];
      var tiles = parent.tiles || {};

      tiles[x] = tiles[x] || {};
      tiles[x][y] = dataUri;

      parent.tiles = tiles;

      continue;
    }

    var surfaceMatches = line.match("(\\s*)Surface: (.*)");
    if (surfaceMatches) {
      var indentation = Math.floor(matches[1].length / 2);
      var parent = objectAtIndentation[indentation - 1] || objectAtIndentation[indentation - 2];

      var surfaceURI = surfaceMatches[2];
      if (parent.surfaceURI != null) {
        console.log("error: surfaceURI already set for this layer " + parent.line);
      }
      parent.surfaceURI = surfaceURI;

      // Look for the buffer-rect offset
      var contentHostLine = layersDumpLines[i - 2].name || layersDumpLines[i - 2];
      var matches = contentHostLine.match(LAYERS_LINE_REGEX);
      if (matches) {
        var contentHostRest = matches[4];
        parent.contentHostProp = {};
        parseProperties(contentHostRest, parent.contentHostProp);
      }

      continue;
    }

    var layerObject = {
      line: line,
      children: [],
    }
    if (!root) {
      root = layerObject;
    }

    var matches = line.match(LAYERS_LINE_REGEX);
    if (!matches) {
      continue; // Something like a texturehost dump. Safe to ignore
    }

    if (matches[2].indexOf("TiledContentHost") != -1 ||
        matches[2].indexOf("ContentHost") != -1 ||
        matches[2].indexOf("ContentClient") != -1 ||
        matches[2].indexOf("MemoryTextureHost") != -1 ||
        matches[2].indexOf("ImageHost") != -1) {
      continue; // We're already pretty good at visualizing these
    }

    var indentation = Math.floor(matches[1].length / 2);
    objectAtIndentation[indentation] = layerObject;
    for (var c = indentation + 1; c < objectAtIndentation.length; c++) {
      objectAtIndentation[c] = null;
    }
    if (indentation > 0) {
      var parent = objectAtIndentation[indentation - 1];
      while (!parent) {
        indentation--;
        parent = objectAtIndentation[indentation - 1];
      }

      parent.children.push(layerObject);
    }

    layerObject.name = matches[2];
    layerObject.address = matches[3];

    var rest = matches[4];

    function parseProperties(rest, layerObject) {
      var fields = [];
      var nesting = 0;
      var startIndex;
      for (var j = 0; j < rest.length; j++) {
        if (rest.charAt(j) == '[') {
          nesting++;
          if (nesting == 1) {
            startIndex = j;
          }
        } else if (rest.charAt(j) == ']') {
          nesting--;
          if (nesting == 0) {
            fields.push(rest.substring(startIndex + 1, j));
          }
        }
      }

      for (var j = 0; j < fields.length; j++) {
        // Something like 'valid=< (x=0, y=0, w=1920, h=2218); >' or 'opaqueContent'
        var field = fields[j];
        //dump("FIELD: " + field + "\n");
        var parts = field.split("=", 2);
        var fieldName = parts[0];
        var rest = field.substring(fieldName.length + 1);
        if (parts.length == 1) {
          layerObject[fieldName] = "true";
          layerObject[fieldName].type = "bool";
          continue;
        }
        var float = parseFloat_cleo(rest); 
        if (float) {
          layerObject[fieldName] = float;
          layerObject[fieldName].type = "float";
          continue;
        }
        var region = parseRegion(rest); 
        if (region) {
          layerObject[fieldName] = region;
          layerObject[fieldName].type = "region";
          continue;
        }
        var rect = parseRect2D(rest);
        if (rect) {
          layerObject[fieldName] = rect;
          layerObject[fieldName].type = "rect2d";
          continue;
        }
        var matrix = parseMatrix2x3(rest);
        if (matrix) {
          layerObject[fieldName] = matrix;
          layerObject[fieldName].type = "matrix2x3";
          continue;
        }
        var color = parseColor(rest);
        if (color) {
          layerObject[fieldName] = color;
          layerObject[fieldName].type = "color";
          continue;
        }
        if (rest[0] == '{' && rest[rest.length - 1] == '}') {
          var object = {};
          parseProperties(rest.substring(1, rest.length - 2).trim(), object);
          layerObject[fieldName] = object;
          layerObject[fieldName].type = "object";
          continue;
        }
        fieldName = fieldName.split(" ")[0];
        layerObject[fieldName] = rest[0];
        layerObject[fieldName].type = "string";
      }
    }
    parseProperties(rest, layerObject);

    if (!layerObject['shadow-transform']) {
      // No shadow transform = identify
      layerObject['shadow-transform'] = [[1, 0], [0, 1], [0, 0]];
    }

    // Compute screenTransformX/screenTransformY
    // TODO Fully support transforms
    if (layerObject['shadow-transform'] && layerObject['transform']) {
      layerObject['screen-transform'] = [layerObject['shadow-transform'][2][0], layerObject['shadow-transform'][2][1]];
      var currIndentation = indentation - 1;
      while (currIndentation >= 0) {
        var transform = objectAtIndentation[currIndentation]['shadow-transform'] || objectAtIndentation[currIndentation]['transform'];
        if (transform) {
          layerObject['screen-transform'][0] += transform[2][0];
          layerObject['screen-transform'][1] += transform[2][1];
        }
        currIndentation--;
      }
    }

    //dump("Fields: " + JSON.stringify(fields) + "\n");
  }
  root.compositeTime = layersDumpLines.compositeTime;
  //dump("OBJECTS: " + JSON.stringify(root) + "\n");
  return root;
}
function populateLayers(root, displayList, pane, previewParent, hasSeenRoot, contentScale, rootPreviewParent) {
  
  contentScale = contentScale || 1;
  rootPreviewParent = rootPreviewParent || previewParent;

  function getDisplayItemForLayer(displayList) {
    var items = [];
    if (!displayList) {
      return items;
    }
    if (displayList.layer == root.address) {
      items.push(displayList);
    }
    for (var i = 0; i < displayList.children.length; i++) {
      var subDisplayItems = getDisplayItemForLayer(displayList.children[i]);
      for (var j = 0; j < subDisplayItems.length; j++) {
        items.push(subDisplayItems[j]);
      }
    }
    return items;
  }
  var elem = createElement("div", {
    className: "layerObjectDescription",
    textContent: root.line,
    style: {
      whiteSpace: "pre",
    },
    onmouseover: function() {
      if (this.layerViewport) {
        this.layerViewport.classList.add("layerHover");
      }
    },
    onmouseout: function() {
      if (this.layerViewport) {
        this.layerViewport.classList.remove("layerHover");
      }
    },
  });
  var icon = createElement("img", {
    src: "show.png",
    style: {
      width: "12px",
      height: "12px",
      marginLeft: "4px",
      marginRight: "4px",
      cursor: "pointer",
    },
    onclick: function() {
      if (this.layerViewport) {
        if (this.layerViewport.style.visibility == "hidden") {
          this.layerViewport.style.visibility = "";
          this.src = "show.png"
        } else {
          this.layerViewport.style.visibility = "hidden";
          this.src = "hide.png"
        }
      }
    }
  });
  elem.insertBefore(icon, elem.firstChild);
  pane.appendChild(elem);

  if (root["shadow-visible"] || root["visible"]) {
    var visibleRegion = root["shadow-visible"] || root["visible"];
    var layerViewport = createElement("div", {
      id: root.address + "_viewport",
      style: {
        position: "absolute",
        pointerEvents: "none",
      },
    });
    elem.layerViewport = layerViewport;
    icon.layerViewport = layerViewport;
    var layerViewportMatrix = [1, 0, 0, 1, 0, 0];
    if (root["shadow-clip"] || root["clip"]) {
      var clip = root["shadow-clip"] || root["clip"]
      var clipElem = createElement("div", {
        id: root.address + "_clip",
        style: {
          left: clip[0]+"px",
          top: clip[1]+"px",
          width: clip[2]+"px",
          height: clip[3]+"px",
          position: "absolute",
          overflow: "hidden",
          pointerEvents: "none",
        },
      });
      layerViewportMatrix[4] += -clip[0];
      layerViewportMatrix[5] += -clip[1];
      layerViewport.style.transform = "translate(-" + clip[0] + "px, -" + clip[1] + "px" + ")";
    }
    if (root["shadow-transform"] || root["transform"]) {
      var matrix = root["shadow-transform"] || root["transform"];
      layerViewportMatrix[0] = matrix[0][0];
      layerViewportMatrix[1] = matrix[0][1];
      layerViewportMatrix[2] = matrix[1][0];
      layerViewportMatrix[3] = matrix[1][1];
      layerViewportMatrix[4] += matrix[2][0];
      layerViewportMatrix[5] += matrix[2][1];
    }
    layerViewport.style.transform = "matrix(" + layerViewportMatrix[0] + "," + layerViewportMatrix[1] + "," + layerViewportMatrix[2] + "," + layerViewportMatrix[3] + "," + layerViewportMatrix[4] + "," + layerViewportMatrix[5] + ")";
    if (!hasSeenRoot) {
      hasSeenRoot = true;
      layerViewport.style.transform = "scale(" + 1/contentScale + "," + 1/contentScale + ")";
    }
    if (clipElem) {
      previewParent.appendChild(clipElem);
      clipElem.appendChild(layerViewport);
    } else {
      previewParent.appendChild(layerViewport);
    }
    previewParent = layerViewport;
    for (var i = 0; i < visibleRegion.length; i++) {
      var rect2d = visibleRegion[i];
      var layerPreview = createElement("div", {
        id: root.address + "_visible_part" + i + "-" + visibleRegion.length,
        className: "layerPreview",
        style: {
          position: "absolute",
          left: rect2d[0] + "px",
          top: rect2d[1] + "px",
          width: rect2d[2] + "px",
          height: rect2d[3] + "px",
          overflow: "hidden",
          border: "solid 1px black",
          background: 'url("noise.png"), linear-gradient(rgba(255, 255, 255, 0.5), rgba(255, 255, 255, 0.2))',
        },
      });
      layerViewport.appendChild(layerPreview);

      function isInside(rect1, rect2) {
        if (rect1[0] + rect1[2] < rect2[0] && rect2[0] + rect2[2] < rect1[0] &&
            rect1[1] + rect1[3] < rect2[1] && rect2[1] + rect2[3] < rect1[1]) {
          return true;
        }
        return true;
      }

      var hasImg = false;
      // Add tile img objects for this part
      var previewOffset = rect2d;

      if (root.tiles) {
        hasImg = true;
        for (var x in root.tiles) {
          for (var y in root.tiles[x]) {
            if (isInside(rect2d, [x, y, 512, 512])) {
              var tileImgElem = createElement("img", {
                src: getDataURI(root.tiles[x][y]),
                style: {
                  position: "absolute",
                  left: (x - previewOffset[0]) + "px",
                  top: (y - previewOffset[1]) + "px",
                  pointerEvents: "auto",
                },
              });
              layerPreview.appendChild(tileImgElem);
            }
          }
        }
        layerPreview.style.background = "";
      } else if (root.surfaceURI) {
        hasImg = true;
        var offsetX = 0;
        var offsetY = 0;
        if (root.contentHostProp && root.contentHostProp['buffer-rect']) {
          offsetX = root.contentHostProp['buffer-rect'][0];
          offsetY = root.contentHostProp['buffer-rect'][1];
        }
        var surfaceImgElem = createElement("img", {
          src: getDataURI(root.surfaceURI),
          style: {
            position: "absolute",
            left: (offsetX - previewOffset[0]) + "px",
            top: (offsetY - previewOffset[1]) + "px",
            pointerEvents: "auto",
          },
        });
        layerPreview.appendChild(surfaceImgElem);
        layerPreview.style.background = "";
      } else if (root.color) {
        hasImg = true;
        layerPreview.style.background = "rgba(" + root.color.r + ", " + root.color.g + ", " + root.color.b + ", " + root.color.a + ")";
      }
      
      if (hasImg || true) {
        layerPreview.mouseoverElem = elem;
        layerPreview.onmouseenter = function() {
          this.mouseoverElem.onmouseover();
        }
        layerPreview.onmouseout = function() {
          this.mouseoverElem.onmouseout();
        }
      }
    }

    var layerDisplayItems = getDisplayItemForLayer(displayList);
    for (var i = 0; i < layerDisplayItems.length; i++) {
      var displayItem = layerDisplayItems[i];
      var displayElem = createElement("div", {
        className: "layerObjectDescription",
        textContent: "            " + trim(displayItem.line),
        style: {
          whiteSpace: "pre",
        },
        displayItem: displayItem,
        layerViewport: layerViewport,
        onmouseover: function() {
          if (this.diPreview) {
            this.diPreview.classList.add("displayHover");

            var description = "";
            if (this.displayItem.contentDescriptor) {
              description += "Content: " + this.displayItem.contentDescriptor;
            } else {
              description += "Content: Unknown";
              }
            description += "<br>Item: " + this.displayItem.name + " (" + this.displayItem.address + ")";
            description += "<br>Layer: " + root.name + " (" + root.address + ")";
            if (this.displayItem.frame) {
              description += "<br>Frame: " + this.displayItem.frame;
            }
            if (this.displayItem.layerBounds) {
              description += "<br>Bounds: [" + toFixed(this.displayItem.layerBounds[0] / 60, 2) + ", " + toFixed(this.displayItem.layerBounds[1] / 60, 2) + ", " + toFixed(this.displayItem.layerBounds[2] / 60, 2) + ", " + toFixed(this.displayItem.layerBounds[3] / 60, 2) + "] (CSS Pixels)";
            }
            if (this.displayItem.z) {
              description += "<br>Z: " + this.displayItem.z;
            }
            // At the end
            if (this.displayItem.rest) {
              description += "<br>" + this.displayItem.rest;
            }

            var box = this.diPreview.getBoundingClientRect();
            var pageBox = document.body.getBoundingClientRect();
            this.diPreview.tooltip = createElement("div", {
              className: "csstooltip",
              innerHTML: description,
              style: {
                top: Math.min(box.bottom, document.documentElement.clientHeight - 150) + "px",
                left: box.left + "px",
              }
            });

            document.body.appendChild(this.diPreview.tooltip);
          }
        },
        onmouseout: function() {
          if (this.diPreview) {
            this.diPreview.classList.remove("displayHover");
            document.body.removeChild(this.diPreview.tooltip);
          }
        },
      });

      var icon = createElement("img", {
        style: {
          width: "12px",
          height: "12px",
          marginLeft: "4px",
          marginRight: "4px",
        }
      });
      displayElem.insertBefore(icon, displayElem.firstChild);
      pane.appendChild(displayElem);
      // bounds doesn't adjust for within the layer. It's not a bad fallback but
      // will have the wrong offset
      var rect2d = displayItem.layerBounds || displayItem.bounds;
      if (rect2d) { // This doesn't place them corectly
        var appUnitsToPixels = 60 / contentScale;
        diPreview = createElement("div", {
          id: "displayitem_" + displayItem.content + "_" + displayItem.address,
          className: "layerPreview",
          style: {
            position: "absolute",
            left: rect2d[0]/appUnitsToPixels + "px",
            top: rect2d[1]/appUnitsToPixels + "px",
            width: rect2d[2]/appUnitsToPixels + "px",
            height: rect2d[3]/appUnitsToPixels + "px",
            border: "solid 1px gray",
            pointerEvents: "auto",
          },
          displayElem: displayElem,
          onmouseover: function() {
            this.displayElem.onmouseover();
          },
          onmouseout: function() {
            this.displayElem.onmouseout();
          },
        });

        layerViewport.appendChild(diPreview);
        displayElem.diPreview = diPreview;
      }
    }
  }

  for (var i = 0; i < root.children.length; i++) {
    populateLayers(root.children[i], displayList, pane, previewParent, hasSeenRoot, contentScale, rootPreviewParent);
  }
}

// This function takes a stdout snippet and finds the frames
function parseMultiLineDump(log) {
  var lines = log.split("\n");

  var container = createElement("div", {
    style: { 
      height: "100%",
      position: "relative",
    },
  });

  var layerManagerFirstLine = "[a-zA-Z]*LayerManager \\(.*$\n";
  var nextLineStartWithSpace = "([ \\t].*$\n)*";
  var layersRegex = "(" + layerManagerFirstLine + nextLineStartWithSpace + ")";

  var startLine = "Painting --- after optimization:\n";
  var endLine = "Painting --- layer tree:"
  var displayListRegex = "(" + startLine + "(.*\n)*?" + endLine + ")";

  var regex = new RegExp(layersRegex + "|" + displayListRegex, "gm");
  var matches = log.match(regex);
  console.log(matches);
  window.matches = matches;

  var matchList = createElement("span", {
    style: { 
      height: "95%",
      width: "10%",
      position: "relative",
      border: "solid black 2px",
      display: "inline-block",
      float: "left",
      overflow: "auto",
    },
  });
  container.appendChild(matchList);
  var contents = createElement("span", {
    style: { 
      height: "95%",
      width: "88%",
      display: "inline-block",
    },
    textContent: "Click on a frame on the left to view the layer tree",
  });
  container.appendChild(contents);

  var lastDisplayList = null;
  var frameID = 1;
  for (let i = 0; i < matches.length; i++) {
    var currMatch = matches[i];

    if (currMatch.indexOf(startLine) == 0) {
      // Display list match
      var matchLines = matches[i].split("\n")
      lastDisplayList = parseDisplayList(matchLines);
    } else {
      // Layer tree match:
      let displayList = lastDisplayList;
      lastDisplayList = null;
      var currFrameDiv = createElement("a", {
        style: {
          padding: "3px",
          display: "block",
        },
        href: "#",
        textContent: "LayerTree " + (frameID++),
        onclick: function() {
          contents.innerHTML = "";
          var matchLines = matches[i].split("\n")
          var dumpDiv = parseDump(matchLines, displayList);
          contents.appendChild(dumpDiv);
        }
      });
      matchList.appendChild(currFrameDiv);
    }
  }

  return container;
}

function parseDump(log, displayList, compositeTitle, compositeTime) {
  compositeTitle |= "";
  compositeTime |= 0

  var container = createElement("div", {
    style: {
      background: "white",
      height: "100%",
      position: "relative",
    },
  });

  if (compositeTitle == null && compositeTime == null) {
    var titleDiv = createElement("div", {
      className: "treeColumnHeader",
      style: {
        width: "100%",
      },
      textContent: compositeTitle + (compositeTitle ? " (near " + compositeTime.toFixed(0) + " ms)" : ""),
    });
    container.appendChild(titleDiv);
  }

  var mainDiv = createElement("div", {
    style: {
      position: "absolute",
      top: "16px",
      left: "0px",
      right: "0px",
      bottom: "0px",
    },
  });
  container.appendChild(mainDiv);

  var layerListPane = createElement("div", {
    style: {
      cssFloat: "left",
      height: "100%",
      width: "300px",
      overflowY: "scroll",
    },
  });
  mainDiv.appendChild(layerListPane);

  var previewDiv = createElement("div", {
    style: {
      position: "absolute",
      left: "300px",
      right: "0px",
      top: "0px",
      bottom: "0px",
      overflow: "auto",
    },
  });
  mainDiv.appendChild(previewDiv);

  var root = parseLayers(log);
  populateLayers(root, displayList, layerListPane, previewDiv);
  return container;
}

function tab_showLayersDump(layersDumpLines, compositeTitle, compositeTime) {
  var container = parseDump(layersDumpLines, compositeTitle, compositeTime);

  gTabWidget.addTab("LayerTree", container); 
  gTabWidget.selectTab("LayerTree");
}

