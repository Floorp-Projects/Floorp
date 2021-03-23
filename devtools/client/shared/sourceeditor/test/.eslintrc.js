"use strict";

// General rule from /.eslintrc.js only accept folders matching **/test*/browser*/
// where is this folder doesn't match, so manually apply browser test config
module.exports = {
  extends: ["plugin:mozilla/browser-test"],
};
