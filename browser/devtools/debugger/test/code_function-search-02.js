/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var test2 = function() {
  // Blah! Second source!
}

var test3 = function test3_NAME() {
}

var test4_SAME_NAME = function test4_SAME_NAME() {
}

test.prototype.x = function X() {
};
test.prototype.sub.y = function Y() {
};
test.prototype.sub.sub.z = function Z() {
};
test.prototype.sub.sub.sub.t = this.x = this.y = this.z = function() {
};
