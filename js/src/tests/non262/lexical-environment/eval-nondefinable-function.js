try {
  eval("var shouldNotBeDefined1; function NaN(){}; var shouldNotBeDefined2;");
} catch (e) {
}

assertEq(Object.getOwnPropertyDescriptor(this, 'shouldNotBeDefined2'), undefined);
assertEq(Object.getOwnPropertyDescriptor(this, 'shouldNotBeDefined1'), undefined);

if (typeof reportCompare === "function")
  reportCompare(true, true);
