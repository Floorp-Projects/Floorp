const { css_beautify } = require("resource://devtools/shared/jsbeautify/src/beautify-css.js");
const {
  html_beautify,
} = require("resource://devtools/shared/jsbeautify/src/beautify-html.js");
const { js_beautify } = require("resource://devtools/shared/jsbeautify/src/beautify-js.js");

exports.css = css_beautify;
exports.html = html_beautify;
exports.js = js_beautify;
