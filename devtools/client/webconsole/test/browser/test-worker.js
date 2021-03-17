"use strict";

self.addEventListener("message", function onMessage(event) {
  const { type, message } = event.data;

  switch (type) {
    case "log":
      console.log(message);
      break;
    case "error":
      throw new Error(message);
  }
});
