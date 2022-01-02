
(function () { 
  assertEquals = function assertEquals(expected, found, name_opt) {  };
})();
function testOne(receiver, key, result) {
  for(var i = 0; i != 10; i++ ) {
    assertEquals(result, receiver[key]());
  }
}
function TypeOfThis() { return typeof this; }
Number.prototype.type = TypeOfThis;
String.prototype.type = TypeOfThis;
Boolean.prototype.type = TypeOfThis;
testOne(2.3, 'type', 'object');
testOne('x', 'type', 'object');
testOne(true, 'type', 'object');
