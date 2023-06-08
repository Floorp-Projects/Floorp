// Calling String.prototype.isWellFormed() or toWellFormed() should throw a
// TypeError when the pref "enable-well-formed-unicode-strings" is off.
//
// This test should be removed once the pref is turned on by default.
assertThrowsInstanceOf(() => "abc".isWellFormed(), TypeError)
assertThrowsInstanceOf(() => "abc".toWellFormed(), TypeError)

if (typeof reportCompare === "function")
  reportCompare(0, 0);
