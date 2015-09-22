var { cssBeautify } = require("devtools/shared/jsbeautify/src/beautify-css");
var { htmlBeautify } = require("devtools/shared/jsbeautify/src/beautify-html");
var { jsBeautify } = require("devtools/shared/jsbeautify/src/beautify-js");

exports.css = cssBeautify;
exports.html = htmlBeautify;
exports.js = jsBeautify;
