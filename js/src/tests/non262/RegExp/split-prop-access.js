var BUGNUMBER = 1287525;
var summary = 'String.prototype.split should call ToUint32(limit) before ToString(separator).';

print(BUGNUMBER + ": " + summary);

var accessed = false;

var rx = /a/;
Object.defineProperty(rx, Symbol.match, {
  get() {
    accessed = true;
  }
});
rx[Symbol.split]("abba");

assertEq(accessed, true);

if (typeof reportCompare === "function")
  reportCompare(true, true);
