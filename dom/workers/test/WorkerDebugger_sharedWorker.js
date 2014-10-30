"use strict";

onconnect = function (event) {
  event.ports[0].onmessage = function (event) {
    switch (event.data) {
    case "close":
      close();
      break;
    }
  };
};
