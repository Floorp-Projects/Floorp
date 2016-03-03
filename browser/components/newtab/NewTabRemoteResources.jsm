/* exported MODE_CHANNEL_MAP */

"use strict";

this.EXPORTED_SYMBOLS = ["MODE_CHANNEL_MAP"];

const MODE_CHANNEL_MAP = {
  "production": {origin: "https://content.cdn.mozilla.net"},
  "staging": {origin: "https://content-cdn.stage.mozaws.net"},
  "test": {origin: "https://example.com"},
  "test2": {origin: "http://mochi.test:8888"},
  "dev": {origin: "http://localhost:8888"}
};
