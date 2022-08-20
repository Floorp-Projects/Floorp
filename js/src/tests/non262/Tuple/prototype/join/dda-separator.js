// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

let dda = createIsHTMLDDA();

assertEq(dda == null, true);
assertEq(dda === null, false);

assertEq(dda == undefined, true);
assertEq(dda === undefined, false);

let tup = Tuple("A", "B");
assertEq(tup.join(null), "A,B");
assertEq(tup.join(undefined), "A,B");
assertEq(tup.join(dda), "A" + dda.toString() + "B");

if (typeof reportCompare === "function")
  reportCompare(0, 0);
