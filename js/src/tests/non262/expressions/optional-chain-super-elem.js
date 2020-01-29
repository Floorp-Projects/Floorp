// Don't assert.

var obj = {
  m() {
    super[0]?.a
  }
};

obj.m();

if (typeof reportCompare === "function")
  reportCompare(true, true);
