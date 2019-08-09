"use strict";

module.exports = {
  "env": {
    "browser": false,
    "mocha": true,
  },

  "globals": {
    // document and window are injected via jsdom-global.
    "document": false,
    "window": false,
  }
}
