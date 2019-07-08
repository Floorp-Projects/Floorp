"use strict";
function a() {
  Cu.reportError(
    "error thrown from test-cu-reporterror.js via Cu.reportError()"
  );
}
a();
