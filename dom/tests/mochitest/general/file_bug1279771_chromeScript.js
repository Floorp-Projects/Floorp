const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;
Components.utils.import("resource://gre/modules/Services.jsm");

// Define these to make EventUtils happy.
let window = this;
let parent = {};

let EventUtils = {};
Services.scriptloader.loadSubScript(
  "chrome://mochikit/content/tests/SimpleTest/EventUtils.js",
  EventUtils
);

let label = browserElement.ownerDocument.getElementById(browserElement.getAttribute('selectmenulist')).querySelector("[label=B]");
EventUtils.synthesizeMouseAtCenter(label, {}, browserElement.ownerDocument.defaultView);
