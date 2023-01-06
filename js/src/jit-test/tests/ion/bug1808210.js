// |jit-test| --ion-offthread-compile=off; --fast-warmup; --blinterp-warmup-threshold=1; --blinterp-eager

let a = {};
let b = {};
class Foo {}
let bar = function() { return new Foo(); };
for (let i = 0; i < 100; i++) {
  for (let j = 0; j < i; j++) {
    b.x + a.x;
    bar();
  }
  for (let k = 0; k < 100; k++) {}
  bar();
}
