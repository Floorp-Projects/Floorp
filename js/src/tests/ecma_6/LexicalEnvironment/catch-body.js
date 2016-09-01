function f() {
  var probeParam, probeBlock;
  let x = 'outside';

  try {
    throw [];
  } catch ([_ = probeParam = function() { return x; }]) {
    probeBlock = function() { return x; };
    let x = 'inside';
  }

  assertEq(probeBlock(), 'inside');
  assertEq(probeParam(), 'outside');
}

f();

if (typeof reportCompare === 'function')
  reportCompare(true, true);
