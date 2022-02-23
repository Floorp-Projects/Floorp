// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

var o1 = { valueOf: function() { throw new SyntaxError(); } };
var o2 = { toString: function() { throw new SyntaxError(); } };

var sample = #[];

assertThrowsInstanceOf(() => sample.slice(0, o1),
                       SyntaxError, "valueOf throws");
assertThrowsInstanceOf(() => sample.slice(0, o2),
                       SyntaxError, "toString throws");

reportCompare(0, 0);
