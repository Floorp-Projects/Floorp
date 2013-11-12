if (typeof ParallelArray === "undefined")
  quit();

x = ParallelArray([1942], function() {})
x + watch.call(x, "length", (function() {}));
