
/*
 * Create a writable property by tracking it with a private variable.
 * We cannot make a normal property writeable on `exports` because
 * the module system freezes it.
 */
function makeWritableFlag(exports, name) {
  let flag = false;
  Object.defineProperty(exports, name, {
    get: function () { return flag; },
    set: function (state) { flag = state; }
  });
}

makeWritableFlag(exports, "wantLogging");
makeWritableFlag(exports, "wantVerbose");

// When the testing flag is set, various behaviors may be altered from
// production mode, typically to enable easier testing or enhanced
// debugging.
makeWritableFlag(exports, "testing");
