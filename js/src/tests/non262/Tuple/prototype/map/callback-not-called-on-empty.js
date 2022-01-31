// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var called = 0;
#[].map(() => called++);
assertEq(called, 0);

reportCompare(0, 0);

