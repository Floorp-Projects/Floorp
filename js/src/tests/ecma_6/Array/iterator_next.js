function testIteratorNextGetLength() {
  var lengthCalledTimes = 0;
  var array = {
    __proto__: Array.prototype,
    get length() {
      lengthCalledTimes += 1;
      return {
        valueOf() {
          return 0;
        }
      };
    }
  };
  var it = array[Symbol.iterator]();
  it.next();
  it.next();
  if (typeof reportCompare === 'function') {
    reportCompare(1, lengthCalledTimes,
      "when an iterator get length zero, it shouldn't access it again");
  }
}
testIteratorNextGetLength();

