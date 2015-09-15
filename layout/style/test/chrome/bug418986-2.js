// # Bug 418986, part 2.

/* jshint esnext:true */
/* jshint loopfunc:true */
/* global window, screen, ok, SpecialPowers, matchMedia */

// Expected values. Format: [name, pref_off_value, pref_on_value]
// If pref_*_value is an array with two values, then we will match
// any value in between those two values. If a value is null, then
// we skip the media query.
var expected_values = [
  ["color", null, 8],
  ["color-index", null, 0],
  ["aspect-ratio", null, window.innerWidth + "/" + window.innerHeight],
  ["device-aspect-ratio", screen.width + "/" + screen.height,
                          window.innerWidth + "/" + window.innerHeight],
  ["device-height", screen.height + "px", window.innerHeight + "px"],
  ["device-width", screen.width + "px", window.innerWidth + "px"],
  ["grid", null, 0],
  ["height", window.innerHeight + "px", window.innerHeight + "px"],
  ["monochrome", null, 0],
  // Square is defined as portrait:
  ["orientation", null,
                  window.innerWidth > window.innerHeight ?
                    "landscape" : "portrait"],
  ["resolution", null, "96dpi"],
  ["resolution", [0.999 * window.devicePixelRatio + "dppx",
                  1.001 * window.devicePixelRatio + "dppx"], "1dppx"],
  ["width", window.innerWidth + "px", window.innerWidth + "px"],
  ["-moz-device-pixel-ratio", window.devicePixelRatio, 1],
  ["-moz-device-orientation", screen.width > screen.height ?
                                "landscape" : "portrait",
                              window.innerWidth > window.innerHeight ?
                                "landscape" : "portrait"]
];

// These media queries return value 0 or 1 when the pref is off.
// When the pref is on, they should not match.
var suppressed_toggles = [
  "-moz-images-in-menus",
  "-moz-mac-graphite-theme",
  // Not available on most OSs.
//  "-moz-maemo-classic",
  "-moz-scrollbar-end-backward",
  "-moz-scrollbar-end-forward",
  "-moz-scrollbar-start-backward",
  "-moz-scrollbar-start-forward",
  "-moz-scrollbar-thumb-proportional",
  "-moz-touch-enabled",
  "-moz-windows-compositor",
  "-moz-windows-default-theme",
  "-moz-windows-glass",
];

// Possible values for '-moz-os-version'
var windows_versions = [
  "windows-xp",
  "windows-vista",
  "windows-win7",
  "windows-win8",
  "windows-win10",
];

// Possible values for '-moz-windows-theme'
var windows_themes = [
  "aero",
  "aero-lite",
  "luna-blue",
  "luna-olive",
  "luna-silver",
  "royale",
  "generic",
  "zune"
];

// Read the current OS.
var OS = SpecialPowers.Services.appinfo.OS;

// If we are using Windows, add an extra toggle only
// available on that OS.
if (OS === "WINNT") {
  suppressed_toggles.push("-moz-windows-classic");
}

// __keyValMatches(key, val)__.
// Runs a media query and returns true if key matches to val.
var keyValMatches = (key, val) => matchMedia("(" + key + ":" + val +")").matches;

// __testMatch(key, val)__.
// Attempts to run a media query match for the given key and value.
// If value is an array of two elements [min max], then matches any
// value in-between.
var testMatch = function (key, val) {
  if (val === null) {
    return;
  } else if (Array.isArray(val)) {
    ok(keyValMatches("min-" + key, val[0]) && keyValMatches("max-" + key, val[1]),
       "Expected " + key + " between " + val[0] + " and " + val[1]);
  } else {
    ok(keyValMatches(key, val), "Expected " + key + ":" + val);
  }
};

// __testToggles(resisting)__.
// Test whether we are able to match the "toggle" media queries.
var testToggles = function (resisting) {
  suppressed_toggles.forEach(
    function (key) {
      var exists = keyValMatches(key, 0) || keyValMatches(key, 1);
      if (resisting) {
         ok(!exists, key + " should not exist.");
      } else {
         ok(exists, key + " should exist.");
      }
    });
};

// __testWindowsSpecific__.
// Runs a media query on the queryName with the given possible matching values.
var testWindowsSpecific = function (resisting, queryName, possibleValues) {
  let foundValue = null;
  possibleValues.forEach(function (val) {
    if (keyValMatches(queryName, val)) {
      foundValue = val;
    }
  });
  if (resisting) {
    ok(!foundValue, queryName + " should have no match");
  } else {
    ok(foundValue, foundValue ? ("Match found: '" + queryName + ":" + foundValue + "'")
                              : "Should have a match for '" + queryName + "'");
  }
};

// __generateHtmlLines(resisting)__.
// Create a series of div elements that look like:
// `<div class='spoof' id='resolution'>resolution</div>`,
// where each line corresponds to a different media query.
var generateHtmlLines = function (resisting) {
  let lines = "";
  expected_values.forEach(
    function ([key, offVal, onVal]) {
      let val = resisting ? onVal : offVal;
      if (val) {
        lines += "<div class='spoof' id='" + key + "'>" + key + "</div>\n";
      }
    });
  suppressed_toggles.forEach(
    function (key) {
      lines += "<div class='suppress' id='" + key + "'>" + key + "</div>\n";
    });
  if (OS === "WINNT") {
    lines += "<div class='windows' id='-moz-os-version'>-moz-os-version</div>";
    lines += "<div class='windows' id='-moz-windows-theme'>-moz-windows-theme</div>";
  }
  return lines;
};

// __cssLine__.
// Creates a line of css that looks something like
// `@media (resolution: 1ppx) { .spoof#resolution { background-color: green; } }`.
var cssLine = function (query, clazz, id, color) {
  return "@media " + query + " { ." + clazz +  "#" + id +
         " { background-color: " + color + "; } }\n";
};

// __constructQuery(key, val)__.
// Creates a CSS media query from key and val. If key is an array of
// two elements, constructs a range query (using min- and max-).
var constructQuery = function (key, val) {
  return Array.isArray(val) ?
    "(min-" + key + ": " + val[0] + ") and (max-" +  key + ": " + val[1] + ")" :
    "(" + key + ": " + val + ")";
};

// __mediaQueryCSSLine(key, val, color)__.
// Creates a line containing a CSS media query and a CSS expression.
var mediaQueryCSSLine = function (key, val, color) {
  if (val === null) {
    return "";
  }
  return cssLine(constructQuery(key, val), "spoof", key, color);
};

// __suppressedMediaQueryCSSLine(key, color)__.
// Creates a CSS line that matches the existence of a
// media query that is supposed to be suppressed.
var suppressedMediaQueryCSSLine = function (key, color, suppressed) {
  let query = "(" + key + ": 0), (" + key + ": 1)";
  return cssLine(query, "suppress", key, color);
};

// __generateCSSLines(resisting)__.
// Creates a series of lines of CSS, each of which corresponds to
// a different media query. If the query produces a match to the
// expected value, then the element will be colored green.
var generateCSSLines = function (resisting) {
  let lines = ".spoof { background-color: red;}\n";
  expected_values.forEach(
    function ([key, offVal, onVal]) {
      lines += mediaQueryCSSLine(key, resisting ? onVal : offVal, "green");
    });
  lines += ".suppress { background-color: " + (resisting ? "green" : "red") + ";}\n";
  suppressed_toggles.forEach(
    function (key) {
      lines += suppressedMediaQueryCSSLine(key, resisting ? "red" : "green");
    });
  if (OS === "WINNT") {
    lines += ".windows { background-color: " + (resisting ? "green" : "red") + ";}\n";
    lines += windows_versions.map(val => "(-moz-os-version: " + val + ")").join(", ") +
             " { #-moz-os-version { background-color: " + (resisting ? "red" : "green") + ";} }\n";
    lines += windows_themes.map(val => "(-moz-windows-theme: " + val + ")").join(",") +
             " { #-moz-windows-theme { background-color: " + (resisting ? "red" : "green") + ";} }\n";
  }
  return lines;
};

// __green__.
// Returns the computed color style corresponding to green.
var green = (function () {
  let temp = document.createElement("span");
  temp.style.backgroundColor = "green";
  return getComputedStyle(temp).backgroundColor;
})();

// __testCSS(resisting)__.
// Creates a series of divs and CSS using media queries to set their
// background color. If all media queries match as expected, then
// all divs should have a green background color.
var testCSS = function (resisting) {
  document.getElementById("display").innerHTML = generateHtmlLines(resisting);
  document.getElementById("test-css").innerHTML = generateCSSLines(resisting);
  let cssTestDivs = document.querySelectorAll(".spoof,.suppress");
  for (let div of cssTestDivs) {
    let color = window.getComputedStyle(div).backgroundColor;
    ok(color === green, "CSS for '" + div.id + "'");
  }
};

// __testOSXFontSmoothing(resisting)__.
// When fingerprinting resistance is enabled, the `getComputedStyle`
// should always return `undefined` for `MozOSXFontSmoothing`.
var testOSXFontSmoothing = function (resisting) {
  let div = document.createElement("div");
  div.style.MozOsxFontSmoothing = "unset";
  let readBack = window.getComputedStyle(div).MozOsxFontSmoothing;
  let smoothingPref = SpecialPowers.getBoolPref("layout.css.osx-font-smoothing.enabled", false);
  is(readBack, resisting ? "" : (smoothingPref ? "auto" : ""),
               "-moz-osx-font-smoothing");
};

// __sleep(timeoutMs)__.
// Returns a promise that resolves after the given timeout.
var sleep = function (timeoutMs) {
  return new Promise(function(resolve, reject) {
    window.setTimeout(resolve);
  });
};

// __testMediaQueriesInPictureElements(resisting)__.
// Test to see if media queries are properly spoofed in picture elements
// when we are resisting fingerprinting. A generator function
// to be used with SpawnTask.js.
var testMediaQueriesInPictureElements = function* (resisting) {
  let lines = "";
  for (let [key, offVal, onVal] of expected_values) {
    let expected = resisting ? onVal : offVal;
    if (expected) {
      let query = constructQuery(key, expected);
      lines += "<picture>\n";
      lines += " <source srcset='/tests/layout/style/test/chrome/match.png' media='" + query + "' />\n";
      lines += " <img title='" + key + ":" + expected + "' class='testImage' src='/tests/layout/style/test/chrome/mismatch.png' alt='" + key + "' />\n";
      lines += "</picture><br/>\n";
    }
  }
  document.getElementById("pictures").innerHTML = lines;
  var testImages = document.getElementsByClassName("testImage");
  yield sleep(0);
  for (let testImage of testImages) {
    ok(testImage.currentSrc.endsWith("/match.png"), "Media query '" + testImage.title + "' in picture should match.");
  }
};

// __pushPref(key, value)__.
// Set a pref value asynchronously, returning a promise that resolves
// when it succeeds.
var pushPref = function (key, value) {
  return new Promise(function(resolve, reject) {
    SpecialPowers.pushPrefEnv({"set": [[key, value]]}, resolve);
  });
};

// __test(isContent)__.
// Run all tests. A generator function to be used
// with SpawnTask.js.
var test = function* (isContent) {
  for (prefValue of [false, true]) {
    yield pushPref("privacy.resistFingerprinting", prefValue);
    let resisting = prefValue && isContent;
    expected_values.forEach(
      function ([key, offVal, onVal]) {
        testMatch(key, resisting ? onVal : offVal);
      });
    testToggles(resisting);
    if (OS === "WINNT") {
      testWindowsSpecific(resisting, "-moz-os-version", windows_versions);
      testWindowsSpecific(resisting, "-moz-windows-theme", windows_themes);
    }
    testCSS(resisting);
    if (OS === "Darwin") {
      testOSXFontSmoothing(resisting);
    }
    yield testMediaQueriesInPictureElements(resisting);
  }
};
