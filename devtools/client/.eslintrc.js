"use strict";

module.exports = {
  "rules": {
    // See bug 1288147, the devtools front-end wants to be able to run in
    // content privileged windows, where ownerGlobal doesn't exist.
    "mozilla/use-ownerGlobal": "off",
  }
};
