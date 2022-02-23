// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

/*
2. If ! IsValidTupleIndex(numericIndex) is false, return empty.
*/

var t = #[1,2,3,4,5];
assertEq(t[true], undefined);
assertEq(t[false], undefined);
assertEq(t[-2], undefined);
/* TODO: This should be undefined as per section 4.1.2.10, step 1,
 * but it's currently treated the same as t[0].
 */
// assertEq(t[-0], undefined);
assertEq(t[10], undefined);
assertEq(t[NaN], undefined);
assertEq(t[Number.POSITIVE_INFINITY], undefined);
assertEq(t[Number.NEGATIVE_INFINITY], undefined);
assertEq(t["abc"], undefined);
assertEq(t["3"], t[3]);
assertEq(t[new String("3")], t[3]);
assertEq(t["0"], t[0]);
assertEq(t[new String("0")], t[0]);
assertEq(t[new Number(0)], t[0]);
assertEq(t[new Number(3)], t[3]);
assertEq(t[1.1], undefined);
assertEq(t[null], undefined);
assertEq(t[undefined], undefined);

var object = {
  valueOf: function() {
    return 1
  }
};
assertEq(t[object], undefined);

var object = {
  valueOf: function() {
    return 1
  },
  toString: function() {
    return 0
  }
};
assertEq(t[object], t[0]);

var object = {
  valueOf: function() {
    return 1
  },
  toString: function() {
    return {}
  }
};
assertEq(t[object], t[1]);

//CHECK#4
try {
  x = [];
  var object = {
    valueOf: function() {
      throw "error"
    },
    toString: function() {
      return 1
    }
  };
  assertEq(tup[object], tup[1]);
}
catch (e) {
  assertEq(e === "error", false);
}

//CHECK#5
var object = {
  toString: function() {
    return 1
  }
};
assertEq(t[object], t[1]);

//CHECK#6
x = [];
var object = {
  valueOf: function() {
    return {}
  },
  toString: function() {
    return 1
  }
}
assertEq(t[object], t[1]);

//CHECK#7
try {
  var object = {
    valueOf: function() {
      return 1
    },
    toString: function() {
      throw "error"
    }
  };
  t[object];
  throw new SyntaxError();
}
catch (e) {
  assertEq(e, "error", 'The value of e is expected to be "error"');
}

//CHECK#8
try {
  var object = {
    valueOf: function() {
      return {}
    },
    toString: function() {
      return {}
    }
  };
  t[object];
  throw new SyntaxError();
}
catch (e) {
  assertEq(e instanceof TypeError, true);
}

reportCompare(0, 0);
