/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["WebVTTParser"];

/**
 * Code below is vtt.js the JS WebVTTParser.
 * Current source code can be found at http://github.com/andreasgal/vtt.js
 *
 * Code taken from commit 355f375b6cf04763dcb1843d5683a7c489846425
 */

(function(global) {

  // Try to parse input as a time stamp.
  function parseTimeStamp(input) {

    function computeSeconds(h, m, s, f) {
      return (h | 0) * 3600 + (m | 0) * 60 + (s | 0) + (f | 0) / 1000;
    }

    var m = input.match(/^(\d+):(\d{2})(:\d{2})?\.(\d{3})/);
    if (!m)
      return null;

    if (m[3]) {
      // Timestamp takes the form of [hours]:[minutes]:[seconds].[milliseconds]
      return computeSeconds(m[1], m[2], m[3].replace(":", ""), m[4]);
    } else if (m[1] > 59) {
      // Timestamp takes the form of [hours]:[minutes].[milliseconds]
      // First position is hours as it's over 59.
      return computeSeconds(m[1], m[2], 0,  m[4]);
    } else {
      // Timestamp takes the form of [minutes]:[seconds].[milliseconds]
      return computeSeconds(0, m[1], m[2], m[4]);
    }
  }

  // A settings object holds key/value pairs and will ignore anything but the first
  // assignment to a specific key.
  function Settings() {
    this.values = Object.create(null);
  }

  Settings.prototype = {
    // Only accept the first assignment to any key.
    set: function(k, v) {
      if (!this.get(k) && v !== "")
        this.values[k] = v;
    },
    // Return the value for a key, or a default value.
    get: function(k, dflt) {
      return this.has(k) ? this.values[k] : dflt;
    },
    // Check whether we have a value for a key.
    has: function(k) {
      return k in this.values;
    },
    // Accept a setting if its one of the given alternatives.
    alt: function(k, v, a) {
      for (var n = 0; n < a.length; ++n) {
        if (v === a[n]) {
          this.set(k, v);
          break;
        }
      }
    },
    // Accept a region if it doesn't have the special string '-->'
    region: function(k, v) {
      if (!v.match(/-->/)) {
        this.set(k, v);
      }
    },
    // Accept a setting if its a valid (signed) integer.
    integer: function(k, v) {
      if (/^-?\d+$/.test(v)) // integer
        this.set(k, parseInt(v, 10));
    },
    // Accept a setting if its a valid percentage.
    percent: function(k, v, frac) {
      var m;
      if ((m = v.match(/^([\d]{1,3})(\.[\d]*)?%$/))) {
        v = v.replace("%", "");
        if (!m[2] || (m[2] && frac)) {
          v = parseFloat(v);
          if (v >= 0 && v <= 100) {
            this.set(k, v);
            return true;
          }
        }
      }
      return false;
    }
  };

  // Helper function to parse input into groups separated by 'groupDelim', and
  // interprete each group as a key/value pair separated by 'keyValueDelim'.
  function parseOptions(input, callback, keyValueDelim, groupDelim) {
    var groups = groupDelim ? input.split(groupDelim) : [input];
    for (var i in groups) {
      var kv = groups[i].split(keyValueDelim);
      if (kv.length !== 2)
        continue;
      var k = kv[0];
      var v = kv[1];
      callback(k, v);
    }
  }

  function parseCue(input, cue) {
    // 4.1 WebVTT timestamp
    function consumeTimeStamp() {
      var ts = parseTimeStamp(input);
      if (ts === null)
        throw "error";
      // Remove time stamp from input.
      input = input.replace(/^[^\s-]+/, "");
      return ts;
    }

    // 4.4.2 WebVTT cue settings
    function consumeCueSettings(input, cue) {
      var settings = new Settings();

      parseOptions(input, function (k, v) {
        switch (k) {
        case "region":
          settings.region(k, v);
          break;
        case "vertical":
          settings.alt(k, v, ["rl", "lr"]);
          break;
        case "line":
          settings.integer(k, v);
          settings.percent(k, v) ? settings.set("snapToLines", false) : null;
          settings.alt(k, v, ["auto"]);
          break;
        case "position":
        case "size":
          settings.percent(k, v);
          break;
        case "align":
          settings.alt(k, v, ["start", "middle", "end", "left", "right"]);
          break;
        }
      }, /:/, /\s/);

      // Apply default values for any missing fields.
      cue.regionId = settings.get("region", "");
      cue.vertical = settings.get("vertical", "");
      cue.line = settings.get("line", "auto");
      cue.snapToLines = settings.get("snapToLines", true);
      cue.position = settings.get("position", 50);
      cue.size = settings.get("size", 100);
      cue.align = settings.get("align", "middle");
    }

    function skipWhitespace() {
      input = input.replace(/^\s+/, "");
    }

    // 4.1 WebVTT cue timings.
    skipWhitespace();
    cue.startTime = consumeTimeStamp();   // (1) collect cue start time
    skipWhitespace();
    if (input.substr(0, 3) !== "-->")     // (3) next characters must match "-->"
      throw "error";
    input = input.substr(3);
    skipWhitespace();
    cue.endTime = consumeTimeStamp();     // (5) collect cue end time

    // 4.1 WebVTT cue settings list.
    skipWhitespace();
    consumeCueSettings(input, cue);
  }

  const ESCAPE = {
    "&amp;": "&",
    "&lt;": "<",
    "&gt;": ">",
    "&lrm;": "\u200e",
    "&rlm;": "\u200f",
    "&nbsp;": "\u00a0"
  };

  const TAG_NAME = {
    c: "span",
    i: "i",
    b: "b",
    u: "u",
    ruby: "ruby",
    rt: "rt",
    v: "span",
    lang: "span"
  };

  const TAG_ANNOTATION = {
    v: "title",
    lang: "lang"
  };

  const NEEDS_PARENT = {
    rt: "ruby"
  };

  // Parse content into a document fragment.
  function parseContent(window, input) {
    function nextToken() {
      // Check for end-of-string.
      if (!input)
        return null;

      // Consume 'n' characters from the input.
      function consume(result) {
        input = input.substr(result.length);
        return result;
      }

      var m = input.match(/^([^<]*)(<[^>]+>?)?/);
      // If there is some text before the next tag, return it, otherwise return
      // the tag.
      return consume(m[1] ? m[1] : m[2]);
    }

    // Unescape a string 's'.
    function unescape1(e) {
      return ESCAPE[e];
    }
    function unescape(s) {
      while ((m = s.match(/&(amp|lt|gt|lrm|rlm|nbsp);/)))
        s = s.replace(m[0], unescape1);
      return s;
    }

    function shouldAdd(current, element) {
      return !NEEDS_PARENT[element.localName] ||
             NEEDS_PARENT[element.localName] === current.localName;
    }

    // Create an element for this tag.
    function createElement(type, annotation) {
      var tagName = TAG_NAME[type];
      if (!tagName)
        return null;
      var element = window.document.createElement(tagName);
      element.localName = tagName;
      var name = TAG_ANNOTATION[type];
      if (name && annotation)
        element[name] = annotation.trim();
      return element;
    }

    var rootDiv = window.document.createElement("div"),
        current = rootDiv,
        t,
        tagStack = [];

    while ((t = nextToken()) !== null) {
      if (t[0] === '<') {
        if (t[1] === "/") {
          // If the closing tag matches, move back up to the parent node.
          if (tagStack.length &&
              tagStack[tagStack.length - 1] === t.substr(2).replace(">", "")) {
            tagStack.pop();
            current = current.parentNode;
          }
          // Otherwise just ignore the end tag.
          continue;
        }
        var ts = parseTimeStamp(t.substr(1, t.length - 2));
        var node;
        if (ts) {
          // Timestamps are lead nodes as well.
          node = window.ProcessingInstruction();
          node.target = "timestamp";
          node.data = ts;
          current.appendChild(node);
          continue;
        }
        var m = t.match(/^<([^.\s/0-9>]+)(\.[^\s\\>]+)?([^>\\]+)?(\\?)>?$/);
        // If we can't parse the tag, skip to the next tag.
        if (!m)
          continue;
        // Try to construct an element, and ignore the tag if we couldn't.
        node = createElement(m[1], m[3]);
        if (!node)
          continue;
        // Determine if the tag should be added based on the context of where it
        // is placed in the cuetext.
        if (!shouldAdd(current, node))
          continue;
        // Set the class list (as a list of classes, separated by space).
        if (m[2])
          node.className = m[2].substr(1).replace('.', ' ');
        // Append the node to the current node, and enter the scope of the new
        // node.
        tagStack.push(m[1]);
        current.appendChild(node);
        current = node;
        continue;
      }

      // Text nodes are leaf nodes.
      current.appendChild(window.document.createTextNode(unescape(t)));
    }

    return rootDiv;
  }

  function computeLinePos(cue) {
    if (typeof cue.line === "number" &&
        (cue.snapToLines || (cue.line >= 0 && cue.line <= 100)))
      return cue.line;
    if (!cue.track)
      return -1;
    // TODO: Have to figure out a way to determine what the position of the
    // Track is in the Media element's list of TextTracks and return that + 1,
    // negated.
    return 100;
  }

  function CueBoundingBox(cue) {
    // TODO: Apply unicode bidi algorithm and assign the result to 'direction'
    this.direction = "ltr";

    var boxLen = (function(direction){
      var maxLen = 0;
      if ((cue.vertical === "" &&
          (cue.align === "left" ||
           (cue.align === "start" && direction === "ltr") ||
           (cue.align === "end" && direction === "rtl"))) ||
         ((cue.vertical === "rl" || cue.vertical === "lr") &&
          (cue.align === "start" || cue.align === "left")))
        maxLen = 100 - cue.position;
      else if ((cue.vertical === "" &&
                (cue.align === "right" ||
                 (cue.align === "end" && direction === "ltr") ||
                 (cue.align === "start" && direction === "rtl"))) ||
               ((cue.vertical === "rl" || cue.vertical === "lr") &&
                 (cue.align === "end" || cue.align === "right")))
        maxLen = cue.position;
      else if (cue.align === "middle") {
        if (cue.position <= 50)
          maxLen = cue.position * 2;
        else
          maxLen = (100 - cue.position) * 2;
      }
      return cue.size < maxLen ? cue.size : maxLen;
    }(this.direction));

    this.left = (function(direction) {
      if (cue.vertical === "") {
        if (direction === "ltr") {
          if (cue.align === "start" || cue.align === "left")
            return cue.position;
          else if (cue.align === "end" || cue.align === "right")
            return cue.position - boxLen;
          else if (cue.align === "middle")
            return cue.position - (boxLen / 2);
        } else if (direction === "rtl") {
          if (cue.align === "end" || cue.align === "left")
            return 100 - cue.position;
          else if (cue.align === "start" || cue.align === "right")
            return 100 - cue.position - boxLen;
          else if (cue.align === "middle")
            return 100 - cue.position - (boxLen / 2);
        }
      }
      return cue.snapToLines ? 0 : computeLinePos(cue);
    }(this.direction));

    this.top = (function() {
      if (cue.vertical === "rl" || cue.vertical === "lr") {
        if (cue.align === "start" || cue.align === "left")
          return cue.position;
        else if (cue.align === "end" || cue.align === "right")
          return cue.position - boxLen;
        else if (cue.align === "middle")
          return cue.position - (boxLen / 2);
      }
      return cue.snapToLines ? 0 : computeLinePos(cue);
    }());

    // Apply a margin to the edges of the bounding box. The margin is user agent
    // defined and is expressed as a percentage of the containing box's width.
    var edgeMargin = 10;
    if (cue.snapToLines) {
      if (cue.vertical === "") {
        if (this.left < edgeMargin && this.left + boxLen > edgeMargin) {
          this.left += edgeMargin;
          boxLen -= edgeMargin;
        }
        var rightMargin = 100 - edgeMargin;
        if (this.left < rightMargin && this.left + boxLen > rightMargin)
          boxLen -= edgeMargin;
      } else if (cue.vertical === "lr" || cue.vertical === "rl") {
        if (this.top < edgeMargin && this.top + boxLen > edgeMargin) {
          this.top += edgeMargin;
          boxLen -= edgeMargin;
        }
        var bottomMargin = 100 - edgeMargin;
        if (this.top < bottomMargin && this.top + boxLen > bottomMargin)
          boxLen -= edgeMargin;
      }
    }

    this.height = cue.vertical === "" ? "auto" : boxLen;
    this.width = cue.vertical === "" ? boxLen : "auto";

    this.writingMode = cue.vertical === "" ?
                       "horizontal-tb" :
                       cue.vertical === "lr" ? "vertical-lr" : "vertical-rl";
    this.position = "absolute";
    this.unicodeBidi = "plaintext";
    this.textAlign = cue.align === "middle" ? "center" : cue.align;
    this.font = "5vh sans-serif";
    this.color = "rgba(255,255,255,1)";
    this.whiteSpace = "pre-line";
  }

  const WEBVTT = "WEBVTT";

  function WebVTTParser(window, decoder) {
    this.window = window;
    this.state = "INITIAL";
    this.buffer = "";
    this.decoder = decoder || TextDecoder("utf8");
  }

  // Helper to allow strings to be decoded instead of the default binary utf8 data.
  WebVTTParser.StringDecoder = function() {
    return {
      decode: function(data) {
        if (!data) return "";
        if (typeof data !== "string") throw "[StringDecoder] Error - expected string data";

        return decodeURIComponent(escape(data));
      }
    };
  };

  WebVTTParser.convertCueToDOMTree = function(window, cuetext) {
    if (!window || !cuetext)
      return null;
    return parseContent(window, cuetext);
  };

  WebVTTParser.processCues = function(window, cues) {
    if (!window || !cues)
      return null;

    return cues.map(function(cue) {
      var div = parseContent(window, cue.text);
      div.style = new CueBoundingBox(cue);
      // TODO: Adjust divs based on other cues already processed.
      // TODO: Account for regions.
      return div;
    });
  };

  WebVTTParser.prototype = {
    parse: function (data) {
      var self = this;

      // If there is no data then we won't decode it, but will just try to parse
      // whatever is in buffer already. This may occur in circumstances, for
      // example when flush() is called.
      if (data) {
        // Try to decode the data that we received.
        self.buffer += self.decoder.decode(data, {stream: true});
      }

      // Advance tells whether or not to remove the collected line from the buffer
      // after it is read.
      function collectNextLine(advance) {
        var buffer = self.buffer;
        var pos = 0;
        advance = typeof advance === "undefined" ? true : advance;
        while (pos < buffer.length && buffer[pos] != '\r' && buffer[pos] != '\n')
          ++pos;
        var line = buffer.substr(0, pos);
        // Advance the buffer early in case we fail below.
        if (buffer[pos] === '\r')
          ++pos;
        if (buffer[pos] === '\n')
          ++pos;
        if (advance)
          self.buffer = buffer.substr(pos);
        return line;
      }

      // 3.4 WebVTT region and WebVTT region settings syntax
      function parseRegion(input) {
        var settings = new Settings();

        parseOptions(input, function (k, v) {
          switch (k) {
          case "id":
            settings.region(k, v);
            break;
          case "width":
            settings.percent(k, v, true);
            break;
          case "lines":
            settings.integer(k, v);
            break;
          case "regionanchor":
          case "viewportanchor":
            var xy = v.split(',');
            if (xy.length !== 2)
              break;
            // We have to make sure both x and y parse, so use a temporary
            // settings object here.
            var anchor = new Settings();
            anchor.percent("x", xy[0], true);
            anchor.percent("y", xy[1], true);
            if (!anchor.has("x") || !anchor.has("y"))
              break;
            settings.set(k + "X", anchor.get("x"));
            settings.set(k + "Y", anchor.get("y"));
            break;
          case "scroll":
            settings.alt(k, v, ["up"]);
            break;
          }
        }, /=/, /\s/);

        // Register the region, using default values for any values that were not
        // specified.
        if (self.onregion && settings.has("id")) {
          var region = new self.window.VTTRegion();
          region.id = settings.get("id");
          region.width = settings.get("width", 100);
          region.lines = settings.get("lines", 3);
          region.regionAnchorX = settings.get("regionanchorX", 0);
          region.regionAnchorY = settings.get("regionanchorY", 100);
          region.viewportAnchorX = settings.get("viewportanchorX", 0);
          region.viewportAnchorY = settings.get("viewportanchorY", 100);
          region.scroll = settings.get("scroll", "none");
          self.onregion(region);
        }
      }

      // 3.2 WebVTT metadata header syntax
      function parseHeader(input) {
        parseOptions(input, function (k, v) {
          switch (k) {
          case "Region":
            // 3.3 WebVTT region metadata header syntax
            parseRegion(v);
            break;
          }
        }, /:/);
      }

      // 5.1 WebVTT file parsing.
      try {
        var line;
        if (self.state === "INITIAL") {
          // Wait until we have enough data to parse the header.
          if (self.buffer.length <= WEBVTT.length)
            return this;

          // Collect the next line, but do not remove the collected line from the
          // buffer as we may not have the full WEBVTT signature yet when
          // incrementally parsing.
          line = collectNextLine(false);
          // (4-12) - Check for the "WEBVTT" identifier followed by an optional space or tab,
          // and ignore the rest of the line.
          if (line.substr(0, WEBVTT.length) !== WEBVTT ||
              line.length > WEBVTT.length && !/[ \t]/.test(line[WEBVTT.length])) {
            throw "error";
          }
          // Now that we've read the WEBVTT signature we can remove it from
          // the buffer.
          collectNextLine(true);
          self.state = "HEADER";
        }

        while (self.buffer) {
          // We can't parse a line until we have the full line.
          if (!/[\r\n]/.test(self.buffer)) {
            // If we are in the midst of parsing a cue, report it early. We will report it
            // again when updates come in.
            if (self.state === "CUETEXT" && self.cue && self.onpartialcue)
              self.onpartialcue(self.cue);
            return this;
          }

          line = collectNextLine();

          switch (self.state) {
          case "HEADER":
            // 13-18 - Allow a header (metadata) under the WEBVTT line.
            if (/:/.test(line)) {
              parseHeader(line);
            } else if (!line) {
              // An empty line terminates the header and starts the body (cues).
              self.state = "ID";
            }
            continue;
          case "NOTE":
            // Ignore NOTE blocks.
            if (!line)
              self.state = "ID";
            continue;
          case "ID":
            // Check for the start of NOTE blocks.
            if (/^NOTE($|[ \t])/.test(line)) {
              self.state = "NOTE";
              break;
            }
            // 19-29 - Allow any number of line terminators, then initialize new cue values.
            if (!line)
              continue;
            self.cue = new self.window.VTTCue(0, 0, "");
            self.state = "CUE";
            // 30-39 - Check if self line contains an optional identifier or timing data.
            if (line.indexOf("-->") == -1) {
              self.cue.id = line;
              continue;
            }
            // Process line as start of a cue.
            /*falls through*/
          case "CUE":
            // 40 - Collect cue timings and settings.
            try {
              parseCue(line, self.cue);
            } catch (e) {
              // In case of an error ignore rest of the cue.
              self.cue = null;
              self.state = "BADCUE";
              continue;
            }
            self.state = "CUETEXT";
            continue;
          case "CUETEXT":
            // 41-53 - Collect the cue text, create a cue, and add it to the output.
            if (!line) {
              // We are done parsing self cue.
              self.oncue && self.oncue(self.cue);
              self.cue = null;
              self.state = "ID";
              continue;
            }
            if (self.cue.text)
              self.cue.text += "\n";
            self.cue.text += line;
            continue;
          default: // BADCUE
            // 54-62 - Collect and discard the remaining cue.
            if (!line) {
              self.state = "ID";
            }
            continue;
          }
        }
      } catch (e) {
        // If we are currently parsing a cue, report what we have, and then the error.
        if (self.state === "CUETEXT" && self.cue && self.oncue)
          self.oncue(self.cue);
        self.cue = null;
        // Report the error and enter the BADCUE state, except if we haven't even made
        // it through the header yet.
        if (self.state !== "INITIAL")
          self.state = "BADCUE";
      }
      return this;
    },
    flush: function () {
      var self = this;
      // Finish decoding the stream.
      self.buffer += self.decoder.decode();
      // Synthesize the end of the current cue or region.
      if (self.cue || self.state === "HEADER") {
        self.buffer += "\n\n";
        self.parse();
      }
      self.onflush && self.onflush();
      return this;
    }
  };

  global.WebVTTParser = WebVTTParser;

}(this));
