// # Bug 1729861

// Expected values. Format: [name, pref_off_value, pref_on_value]
var expected_values = [
  [
    "device-aspect-ratio",
    screen.width + "/" + screen.height,
    window.innerWidth + "/" + window.innerHeight,
  ],
  ["device-height", screen.height + "px", window.innerHeight + "px"],
  ["device-width", screen.width + "px", window.innerWidth + "px"],
];

// Create a line containing a CSS media query and a rule to set the bg color.
var mediaQueryCSSLine = function (key, val, color) {
  return (
    "@media (" +
    key +
    ": " +
    val +
    ") { #" +
    key +
    " { background-color: " +
    color +
    "; } }\n"
  );
};

var green = "rgb(0, 128, 0)";
var blue = "rgb(0, 0, 255)";

// Set a pref value asynchronously, returning a promise that resolves
// when it succeeds.
var pushPref = function (key, value) {
  return SpecialPowers.pushPrefEnv({ set: [[key, value]] });
};

// Set the resistFingerprinting pref to the given value, and then check that the background
// color has been updated properly as a result of re-evaluating the media queries.
var checkColorForPref = async function (setting, testDivs, expectedColor) {
  await pushPref("privacy.resistFingerprinting", setting);
  for (let div of testDivs) {
    let color = window.getComputedStyle(div).backgroundColor;
    is(color, expectedColor, "background for '" + div.id + "' is " + color);
  }
};

var test = async function () {
  // If the "off" and "on" expected values are the same, we can't actually
  // test anything here. (Might this be the case on mobile?)
  let skipTest = false;
  for (let [key, offVal, onVal] of expected_values) {
    if (offVal == onVal) {
      todo(false, "Unable to test because '" + key + "' is invariant");
      return;
    }
  }

  let css =
    ".test { margin: 1em; padding: 1em; color: white; width: max-content; font: 2em monospace }\n";

  // Create a test element for each of the media queries we're checking, and append the matching
  // "on" and "off" media queries to the CSS.
  let testDivs = [];
  for (let [key, offVal, onVal] of expected_values) {
    let div = document.createElement("div");
    div.textContent = key;
    div.setAttribute("class", "test");
    div.setAttribute("id", key);
    testDivs.push(div);
    document.getElementById("display").appendChild(div);
    css += mediaQueryCSSLine(key, onVal, "green");
    css += mediaQueryCSSLine(key, offVal, "blue");
  }
  document.getElementById("test-css").textContent = css;

  // Check that the test elements change color as expected when we flip the resistFingerprinting pref.
  await checkColorForPref(true, testDivs, green);
  await checkColorForPref(false, testDivs, blue);
  await checkColorForPref(true, testDivs, green);
};
