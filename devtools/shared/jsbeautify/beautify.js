const { css_beautify } = require("devtools/shared/jsbeautify/src/beautify-css");
const {
  html_beautify,
} = require("devtools/shared/jsbeautify/src/beautify-html");
const { js_beautify } = require("devtools/shared/jsbeautify/src/beautify-js");

exports.css = css_beautify;
exports.html = html_beautify;
exports.js = js_beautify;
