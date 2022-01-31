// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var sample = #[1,2,3];

assertThrowsInstanceOf(() => sample.map(function () { throw new TypeError("monkeys"); }),
                       TypeError,
                       "should throw TypeError: monkeys");

reportCompare(0, 0);
