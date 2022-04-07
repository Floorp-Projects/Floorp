// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array-exotic-objects-defineownproperty-p-desc
info: Set the value of property length of A to Uint32(length)
es5id: 15.4.5.1_A1.3_T2
description: Uint32 use ToNumber and ToPrimitve
---*/

//CHECK#1
var x = [];
x.length = {
  valueOf: function() {
    return 2
  }
};
assert.sameValue(x.length, 2, 'The value of x.length is expected to be 2');

//CHECK#2
x = [];
x.length = {
  valueOf: function() {
    return 2
  },
  toString: function() {
    return 1
  }
};
assert.sameValue(x.length, 2, 'The value of x.length is expected to be 2');

//CHECK#3
x = [];
x.length = {
  valueOf: function() {
    return 2
  },
  toString: function() {
    return {}
  }
};
assert.sameValue(x.length, 2, 'The value of x.length is expected to be 2');

//CHECK#4
try {
  x = [];
  x.length = {
    valueOf: function() {
      return 2
    },
    toString: function() {
      throw "error"
    }
  };
  assert.sameValue(x.length, 2, 'The value of x.length is expected to be 2');
}
catch (e) {
  assert.notSameValue(e, "error", 'The value of e is not "error"');
}

//CHECK#5
x = [];
x.length = {
  toString: function() {
    return 1
  }
};
assert.sameValue(x.length, 1, 'The value of x.length is expected to be 1');

//CHECK#6
x = [];
x.length = {
  valueOf: function() {
    return {}
  },
  toString: function() {
    return 1
  }
}
assert.sameValue(x.length, 1, 'The value of x.length is expected to be 1');

//CHECK#7
try {
  x = [];
  x.length = {
    valueOf: function() {
      throw "error"
    },
    toString: function() {
      return 1
    }
  };
  x.length;
  throw new Test262Error('#7.1: x = []; x.length = {valueOf: function() {throw "error"}, toString: function() {return 1}}; x.length throw "error". Actual: ' + (x.length));
}
catch (e) {
  assert.sameValue(e, "error", 'The value of e is expected to be "error"');
}

//CHECK#8
try {
  x = [];
  x.length = {
    valueOf: function() {
      return {}
    },
    toString: function() {
      return {}
    }
  };
  x.length;
  throw new Test262Error('#8.1: x = []; x.length = {valueOf: function() {return {}}, toString: function() {return {}}}  x.length throw TypeError. Actual: ' + (x.length));
}
catch (e) {
  assert.sameValue(
    e instanceof TypeError,
    true,
    'The result of evaluating (e instanceof TypeError) is expected to be true'
  );
}

reportCompare(0, 0);
