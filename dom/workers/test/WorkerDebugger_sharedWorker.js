"use strict";

self.onconnect = function (event) {
  event.ports[0].onmessage = function (e) {
    switch (e.data) {
    case "close":
      close();
      break;

    case "close_loop":
      close();
      // Let's loop forever.
      while(1) {}
      break;
    }
  };
};
