// Create a derived class with a default class constructor.
class C extends class {} {}

// The default constructor of a derived class is small enough to be inlinable.
assertEq(isSmallFunction(C), true);

// Bound functions have a configurable "prototype" property.
const BF = function(){}.bind();

function testBoundFunction() {
  for (let i = 0; i <= 1000; ++i) {
    let newTarget = i < 1000 ? C : BF;
    Reflect.construct(C, [], newTarget);
  }
}

for (let i = 0; i < 2; ++i) testBoundFunction();

// Proxy have a configurable "prototype" property.
const P = new Proxy(function(){}, {});

function testProxy() {
  for (let i = 0; i <= 1000; ++i) {
    let newTarget = i < 1000 ? C : P;
    Reflect.construct(C, [], newTarget);
  }
}

for (let i = 0; i < 2; ++i) testProxy();
