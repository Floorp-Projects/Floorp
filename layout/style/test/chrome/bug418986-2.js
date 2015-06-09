// # Bug 418986, part 2.

/* jshint esnext:true */
/* jshint loopfunc:true */
/* global window, screen, ok, SpecialPowers, matchMedia */

SimpleTest.waitForExplicitFinish();

// Expected values. Format: [name, pref_off_value, pref_on_value]
// If pref_*_value is an array with two values, then we will match
// any value in between those two values. If a value is null, then
// we skip the media query.
let expected_values = [
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
let suppressed_toggles = [
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
let windows_versions = [
  "windows-xp",
  "windows-vista",
  "windows-win7",
  "windows-win8"];

// Possible values for '-moz-windows-theme'
let windows_themes = [
  "aero",
  "luna-blue",
  "luna-olive",
  "luna-silver",
  "royale",
  "generic",
  "zune"
];

// Read the current OS.
let OS = SpecialPowers.Services.appinfo.OS;

// If we are using Windows, add an extra toggle only
// available on that OS.
if (OS === "WINNT") {
  suppressed_toggles.push("-moz-windows-classic");
}

// __keyValMatches(key, val)__.
// Runs a media query and returns true if key matches to val.
let keyValMatches = (key, val) => matchMedia("(" + key + ":" + val +")").matches;

// __testMatch(key, val)__.
// Attempts to run a media query match for the given key and value.
// If value is an array of two elements [min max], then matches any
// value in-between.
let testMatch = function (key, val) {
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
let testToggles = function (resisting) {
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
let testWindowsSpecific = function (resisting, queryName, possibleValues) {
  let found = false;
  possibleValues.forEach(function (val) {
    found = found || keyValMatches(queryName, val);
  });
  if (resisting) {
    ok(!found, queryName + " should have no match");
  } else {
    ok(found, queryName + " should match");
  }
};

// __generateHtmlLines(resisting)__.
// Create a series of div elements that look like:
// `<div class='spoof' id='resolution'>resolution</div>`,
// where each line corresponds to a different media query.
let generateHtmlLines = function (resisting) {
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
let cssLine = function (query, clazz, id, color) {
  return "@media " + query + " { ." + clazz +  "#" + id +
         " { background-color: " + color + "; } }\n";
};

// __mediaQueryCSSLine(key, val, color)__.
// Creates a line containing a CSS media query and a CSS expression.
let mediaQueryCSSLine = function (key, val, color) {
  if (val === null) {
    return "";
  }
  let query;
  if (Array.isArray(val)) {
    query = "(min-" + key + ": " + val[0] + ") and (max-" +  key + ": " + val[1] + ")";
  } else {
    query = "(" + key + ": " + val + ")";
  }
  return cssLine(query, "spoof", key, color);
};

// __suppressedMediaQueryCSSLine(key, color)__.
// Creates a CSS line that matches the existence of a
// media query that is supposed to be suppressed.
let suppressedMediaQueryCSSLine = function (key, color, suppressed) {
  let query = "(" + key + ": 0), (" + key + ": 1)";
  return cssLine(query, "suppress", key, color);
};

// __generateCSSLines(resisting)__.
// Creates a series of lines of CSS, each of which corresponds to
// a different media query. If the query produces a match to the
// expected value, then the element will be colored green.
let generateCSSLines = function (resisting) {
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
let green = (function () {
  let temp = document.createElement("span");
  temp.style.backgroundColor = "green";
  return getComputedStyle(temp).backgroundColor;
})();

// __testCSS(resisting)__.
// Creates a series of divs and CSS using media queries to set their
// background color. If all media queries match as expected, then
// all divs should have a green background color.
let testCSS = function (resisting) {
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
let testOSXFontSmoothing = function (resisting) {
  let div = document.createElement("div");
  div.style.MozOsxFontSmoothing = "unset";
  let readBack = window.getComputedStyle(div).MozOsxFontSmoothing;
  let smoothingPref = SpecialPowers.getBoolPref("layout.css.osx-font-smoothing.enabled", false);
  is(readBack, resisting ? "" : (smoothingPref ? "auto" : ""),
               "-moz-osx-font-smoothing");
};

// An iterator yielding pref values for two consecutive tests.
let prefVals = (for (prefVal of [false, true]) prefVal);

// __test(isContent)__.
// Run all tests.
let test = function(isContent) {
  let {value: prefValue, done} = prefVals.next();
  if (done) {
    SimpleTest.finish();
    return;
  }
  SpecialPowers.pushPrefEnv({set: [["privacy.resistFingerprinting", prefValue]]},
    function () {
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
      test(isContent);
    });
};
