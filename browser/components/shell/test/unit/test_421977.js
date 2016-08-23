var Cc = Components.classes;
var Ci = Components.interfaces;
var Cr = Components.results;

const GCONF_BG_COLOR_KEY = "/desktop/gnome/background/primary_color";

var gShell;
var gGConf;

/**
 * Converts from a rgb numerical color valule (r << 16 | g << 8 | b)
 * into a hex string in #RRGGBB format.
 */
function colorToHex(aColor) {
  const rMask = 4294901760;
  const gMask = 65280;
  const bMask = 255;

  var r = (aColor & rMask) >> 16;
  var g = (aColor & gMask) >> 8;
  var b = (aColor & bMask);

  return "#" + [r, g, b].map(aInt =>
                              aInt.toString(16).replace(/^(.)$/, "0$1"))
                             .join("").toUpperCase();
}

/**
 * Converts a color string in #RRGGBB format to a rgb numerical color value
 *  (r << 16 | g << 8 | b).
 */
function hexToColor(aString) {
  return parseInt(aString.substring(1, 3), 16) << 16 |
         parseInt(aString.substring(3, 5), 16) << 8 |
         parseInt(aString.substring(5, 7), 16);
}

/**
 * Checks that setting the GConf background key to aGConfColor will
 * result in the Shell component returning a background color equals
 * to aExpectedShellColor in #RRGGBB format.
 */
function checkGConfToShellColor(aGConfColor, aExpectedShellColor) {

  gGConf.setString(GCONF_BG_COLOR_KEY, aGConfColor);
  var shellColor = colorToHex(gShell.desktopBackgroundColor);

  do_check_eq(shellColor, aExpectedShellColor);
}

/**
 * Checks that setting the background color (in #RRGGBB format) using the Shell
 * component will result in having a GConf key for the background color set to
 * aExpectedGConfColor.
 */
function checkShellToGConfColor(aShellColor, aExpectedGConfColor) {

  gShell.desktopBackgroundColor = hexToColor(aShellColor);
  var gconfColor = gGConf.getString(GCONF_BG_COLOR_KEY);

  do_check_eq(gconfColor, aExpectedGConfColor);
}

function run_test() {

  // This test is Linux specific for now
  if (!("@mozilla.org/gnome-gconf-service;1" in Cc))
    return;

  try {
    // If GSettings is available, then the GConf tests
    // will fail
    var gsettings = Cc["@mozilla.org/gsettings-service;1"].
                    getService(Ci.nsIGSettingsService).
                    getCollectionForSchema("org.gnome.desktop.background");
    return;
  } catch (e) { }

  gGConf = Cc["@mozilla.org/gnome-gconf-service;1"].
           getService(Ci.nsIGConfService);

  gShell = Cc["@mozilla.org/browser/shell-service;1"].
           getService(Ci.nsIShellService);

  // Save the original background color so that we can restore it
  // after the test.
  var origGConfColor = gGConf.getString(GCONF_BG_COLOR_KEY);

  try {

    checkGConfToShellColor("#000", "#000000");
    checkGConfToShellColor("#00f", "#0000FF");
    checkGConfToShellColor("#b2f", "#BB22FF");
    checkGConfToShellColor("#fff", "#FFFFFF");

    checkGConfToShellColor("#000000", "#000000");
    checkGConfToShellColor("#0000ff", "#0000FF");
    checkGConfToShellColor("#b002f0", "#B002F0");
    checkGConfToShellColor("#ffffff", "#FFFFFF");

    checkGConfToShellColor("#000000000", "#000000");
    checkGConfToShellColor("#00f00f00f", "#000000");
    checkGConfToShellColor("#aaabbbccc", "#AABBCC");
    checkGConfToShellColor("#fffffffff", "#FFFFFF");

    checkGConfToShellColor("#000000000000", "#000000");
    checkGConfToShellColor("#000f000f000f", "#000000");
    checkGConfToShellColor("#00ff00ff00ff", "#000000");
    checkGConfToShellColor("#aaaabbbbcccc", "#AABBCC");
    checkGConfToShellColor("#111122223333", "#112233");
    checkGConfToShellColor("#ffffffffffff", "#FFFFFF");

    checkShellToGConfColor("#000000", "#000000000000");
    checkShellToGConfColor("#0000FF", "#00000000ffff");
    checkShellToGConfColor("#FFFFFF", "#ffffffffffff");
    checkShellToGConfColor("#0A0B0C", "#0a0a0b0b0c0c");
    checkShellToGConfColor("#A0B0C0", "#a0a0b0b0c0c0");
    checkShellToGConfColor("#AABBCC", "#aaaabbbbcccc");

  } finally {
    gGConf.setString(GCONF_BG_COLOR_KEY, origGConfColor);
  }
}
