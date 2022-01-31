// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/* description: Tuple toSorted does cast values to String */

var toStringCalled = false;
Number.prototype.toString = function() {
  toStringCalled = true;
}

let sample = #[20, 100, 3];
let result = sample.toSorted();
assertEq(toStringCalled, false);
assertEq(result, #[100, 20, 3]);

reportCompare(0, 0);
