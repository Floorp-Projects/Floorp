// Do not assert.

var s = "a";
var b = 32767;

for (var i = 0; i < 10; ++i) {
  b = b & s.charCodeAt();
}
