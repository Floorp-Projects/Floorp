this.x = [];
Function.apply(null, this.x);
Object.defineProperty(this, "x", {get: valueOf});
assertEq(evaluate("this.x;"), this);
