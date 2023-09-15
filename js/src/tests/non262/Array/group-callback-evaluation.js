var array = [1, 2, 3];

var calls = 0;

var grouped = Object.groupBy(array, () => {
  calls++;

  return {
    toString() {
      return "a";
    }
  }
});

assertEq(calls, 3);

if (typeof reportCompare === "function")
  reportCompare(0, 0);
