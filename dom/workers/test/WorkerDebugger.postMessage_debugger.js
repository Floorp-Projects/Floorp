"use strict"

onmessage = function (event) {
  switch (event.data) {
  case "ping":
    postMessage("pong");
    break;
  }
};
