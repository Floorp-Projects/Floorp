"use strict"

this.onmessage = function (event) {
  switch (event.data) {
  case "ping":
    postMessage("pong");
    break;
  }
};
