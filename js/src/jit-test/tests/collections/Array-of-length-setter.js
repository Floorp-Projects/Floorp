// Array.of calls a "length" setter if one is present.

var hits = 0;
var lastObj = null, lastVal = undefined;
function setter(v) {
    hits++;
    lastObj = this;
    lastVal = v;
}

// when the setter is on the new object
function Pack() {
    Object.defineProperty(this, "length", {set: setter});
}
Pack.of = Array.of;
var pack = Pack.of("wolves", "cards", "cigarettes", "lies");
assertEq(lastObj, pack);
assertEq(lastVal, 4);

// when the setter is on the new object's prototype
function Bevy() {}
Object.defineProperty(Bevy.prototype, "length", {set: setter});
Bevy.of = Array.of;
var bevy = Bevy.of("quail");
assertEq(lastObj, bevy);
assertEq(lastVal, 1);
