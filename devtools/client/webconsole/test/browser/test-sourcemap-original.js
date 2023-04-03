// prettier-ignore

/**
 * this
 * is
 * a
 * function
 */
function logString(str) {
  console.log(str);
}

function logTrace() {
  var logTraceInner = function() {
    console.trace();
  };
  logTraceInner();
}
