// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
Object.defineProperty(Tuple.prototype, "length", {value: 0});
let t = #[1,2,3];
var result = t.map(x => x + 1);
// overriding length shouldn't have an effect
assertEq(result, #[2,3,4]);


Object.defineProperty(Tuple.prototype, "length", {
  get() { return 0 }
});
let u = #[1,2,3];
var result = u.map(x => x + 1);
// overriding length shouldn't have an effect
assertEq(result, #[2,3,4]);

reportCompare(0, 0);

