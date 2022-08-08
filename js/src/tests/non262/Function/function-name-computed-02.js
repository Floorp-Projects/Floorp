var obj = {
  [1]: function() {},
  [2]: function*() {},
  [3]: async function() {},
  [4]: async function*() {},
  [5]: ()=>{},
  [6]: async ()=>{},
  [7] () {},
  [8]: class {},
  [9]: function F() {},
  [10]: class C{},
  get [11]() {},
  set [12](x) {},
};

assertEq(obj[1].name, "1");
assertEq(obj[2].name, "2");
assertEq(obj[3].name, "3");
assertEq(obj[4].name, "4");
assertEq(obj[5].name, "5");
assertEq(obj[6].name, "6");
assertEq(obj[7].name, "7");
assertEq(obj[8].name, "8");
assertEq(obj[9].name, "F");
assertEq(obj[10].name, "C");
assertEq(Object.getOwnPropertyDescriptor(obj, "11").get.name, "get 11");
assertEq(Object.getOwnPropertyDescriptor(obj, "12").set.name, "set 12");

let dummy = class {
  [1]() {}
  *[2]() {}
  async [3]() {}
  async *[4]() {}
  [5] = ()=>{}
  [6] = async ()=>{};
  [7] () {}
  get [11]() {}
  set [12](x) {}
};

dum = new dummy();

assertEq(dum[1].name, "1");
assertEq(dum[2].name, "2");
assertEq(dum[3].name, "3");
assertEq(dum[4].name, "4");
assertEq(dum[5].name, "5");
assertEq(dum[6].name, "6");
assertEq(dum[7].name, "7");

assertEq(Object.getOwnPropertyDescriptor(dummy.prototype, "11").get.name, "get 11");
assertEq(Object.getOwnPropertyDescriptor(dummy.prototype, "12").set.name, "set 12");


if (typeof reportCompare === "function")
  reportCompare(true, true);

