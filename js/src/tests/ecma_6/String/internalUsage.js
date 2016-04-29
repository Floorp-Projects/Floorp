var BUGNUMBER = 1268138;
var summary = 'Internal usage of split should not be affected by prototpe change';

print(BUGNUMBER + ": " + summary);

function test() {
  var t = 24*60*60*1000;
  var possibleAnswer = ["1.1.1970", "2.1.1970", "3.1.1970"];

  String.prototype[Symbol.split] = function(s, limit) { return [""]; };
  var s = Intl.DateTimeFormat("de", {}).format(t);
  assertEq(possibleAnswer.includes(s), true);

  String.prototype[Symbol.split] = function(s, limit) { return ["x-foo"]; };
  s = Intl.DateTimeFormat("de", {}).format(t);
  assertEq(possibleAnswer.includes(s), true);

  String.prototype[Symbol.split] = function(s, limit) { return ["de-u-co"]; };
  s = Intl.DateTimeFormat("de", {}).format(t);
  assertEq(possibleAnswer.includes(s), true);

  String.prototype[Symbol.split] = function(s, limit) { return ["en-US"]; };
  s = Intl.DateTimeFormat("de", {}).format(t);
  assertEq(possibleAnswer.includes(s), true);
}

if (this.hasOwnProperty("Intl"))
  test();

reportCompare(0, 0, "ok");
