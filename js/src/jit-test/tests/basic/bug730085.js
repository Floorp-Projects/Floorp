load(libdir + "asserts.js");
var calledToString = false;
assertThrowsInstanceOf(function () { Object.prototype.hasOwnProperty.call(null,
                      {toString: function () { calledToString = true; }}); },
                       TypeError);
assertEq(calledToString, true);
