var a = 6;
Object.defineProperty(this, "a", {writable: false});
a = 7;
assertEq(a, 6);
