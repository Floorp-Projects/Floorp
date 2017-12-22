// Make sure that we don't misorder subclassing accesses with respect to
// accessing regex arg internal slots
//
// Test credit André Bargull.

var re = /a/;
var newRe = Reflect.construct(RegExp, [re], Object.defineProperty(function(){}.bind(null), "prototype", {
  get() {
    re.compile("b");
    return RegExp.prototype;
  }
}));
assertEq(newRe.source, "a");

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
