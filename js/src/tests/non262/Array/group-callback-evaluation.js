// |reftest| shell-option(--enable-array-grouping) skip-if(!Array.prototype.group)

var array = [1, 2, 3];

var calls = 0;

var grouped = array.group(() => {
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
