// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

var obj = {
  0: 2,
  1: 4,
  2: 8,
  3: 16
}

var t = Tuple.from(obj);
assertEq(t.length, 0);

reportCompare(0, 0);
