// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

var o1 = { valueOf: function() { throw new SyntaxError(); } };
var o2 = { toString: function() { throw new SyntaxError(); } };

var sample = #[];

assertThrowsInstanceOf(() => sample.slice(o1),
                       SyntaxError, "valueOf throws");
assertThrowsInstanceOf(() => sample.slice(o2),
                       SyntaxError, "toString throws");

reportCompare(0, 0);
