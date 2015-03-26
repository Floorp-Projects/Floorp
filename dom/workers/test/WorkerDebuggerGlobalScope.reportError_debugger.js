"use strict";

this.onmessage = function (event) {
  switch (event.data) {
  case "report":
    reportError("reported");
    break;
  case "throw":
    throw new Error("thrown");
    break;
  }
};
