// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

assertEq(isConstructor(Tuple.prototype.slice), false);

assertThrowsInstanceOf(() => {
  let t = #[1];
  new t.slice();
}, TypeError, '`let t = #[1]; new t.slice()` throws TypeError');

reportCompare(0, 0);
