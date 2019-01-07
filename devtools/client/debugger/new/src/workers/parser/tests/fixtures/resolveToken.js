const a = 1;
let b = 0;

function getA() {
  return a;
}

function setB(newB) {
  b = newB;
}

const plusAB = (function(x, y) {
  const obj = { x, y };
  function insideClosure(alpha, beta) {
    return alpha + beta + obj.x + obj.y;
  }

  return insideClosure;
})(a, b);

function withMultipleScopes() {
  var outer = 1;
  function innerScope() {
    var inner = outer + 1;
    return inner;
  }

  const fromIIFE = (function(toIIFE) {
    return innerScope() + toIIFE;
  })(1);

  {
    // random block
    let x = outer + fromIIFE;
    if (x) {
      const y = x * x;
      console.log(y);
    }
  }
}
