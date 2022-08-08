var obj = {
  ["func"]: function() {},
  ["genFunc"]: function*() {},
  ["asyncFunc"]: async function() {},
  ["asyncGenFunc"]: async function*() {},
  ["arrowFunc"]: ()=>{},
  ["asyncArrowFunc"]: async ()=>{},
  ["method"]() {},
  ["anonClass"]: class {},
  ["nonAnonymousFunc"]: function F() {},
  ["nonAnonymousClass"]: class C{},
  get ["getter"]() {},
  set ["setter"](x) {},
};

assertEq(obj.func.name, "func");
assertEq(obj.genFunc.name, "genFunc");
assertEq(obj.asyncFunc.name, "asyncFunc");
assertEq(obj.asyncGenFunc.name, "asyncGenFunc");
assertEq(obj.arrowFunc.name, "arrowFunc");
assertEq(obj.asyncArrowFunc.name, "asyncArrowFunc");
assertEq(obj.method.name, "method");
assertEq(obj.anonClass.name, "anonClass");
assertEq(obj.nonAnonymousFunc.name, "F");
assertEq(obj.nonAnonymousClass.name, "C");

assertEq(Object.getOwnPropertyDescriptor(obj, "getter").get.name, "get getter");
assertEq(Object.getOwnPropertyDescriptor(obj, "setter").set.name, "set setter");

let dummy = class {
  ["func"]() {}
  *["genFunc"] () {}
  async ["asyncFunc"]() {}
  async *["asyncGenFunc"]() {}
  ["arrowFunc"] = ()=>{}
  ["asyncArrowFunc"] = async ()=>{};
  ["method"]() {}
  get ["getter"]() {}
  set ["setter"](x) {}
};

dum = new dummy();

assertEq(dum.func.name, "func");
assertEq(dum.genFunc.name, "genFunc");
assertEq(dum.asyncFunc.name, "asyncFunc");
assertEq(dum.asyncGenFunc.name, "asyncGenFunc");
assertEq(dum.arrowFunc.name, "arrowFunc");
assertEq(dum.asyncArrowFunc.name, "asyncArrowFunc");
assertEq(dum.method.name, "method");

assertEq(Object.getOwnPropertyDescriptor(dummy.prototype, "getter").get.name, "get getter");
assertEq(Object.getOwnPropertyDescriptor(dummy.prototype, "setter").set.name, "set setter");

if (typeof reportCompare === "function")
  reportCompare(true, true);

