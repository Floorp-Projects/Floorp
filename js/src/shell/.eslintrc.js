"use strict";

module.exports = {
  "plugins": [
    "spidermonkey-js"
  ],

  "overrides": [{
    "files": ["*.js"],
    "processor": "spidermonkey-js/processor",
  }],
};
