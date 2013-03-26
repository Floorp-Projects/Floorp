/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

window.addEventListener("bogus", function namedEventListener() {
  // Blah! Third source!
});

try {
  var bar = foo.sub.sub.test({
    a: function A() {
    }
  });

  bar.alpha = foo.sub.sub.test({
    b: function B() {
    }
  });

  bar.alpha.beta = new X(Y(Z(foo.sub.sub.test({
    c: function C() {
    }
  }))));

  this.theta = new X(new Y(new Z(new foo.sub.sub.test({
    d: function D() {
    }
  }))));

  var fun = foo = bar = this.t_foo = window.w_bar = function baz() {};

} catch (e) {
}
