"use strict";

self.addEventListener("message", function(event) {
  switch (event.data) {
  case "ping":
    self.postMessage("pong");
    break;
  }
});
