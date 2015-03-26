"use strict";

self.onerror = function () {
  postMessage("error");
}
