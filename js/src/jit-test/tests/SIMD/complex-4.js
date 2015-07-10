load(libdir + 'simd.js');

if (typeof SIMD === "undefined")
    quit();

setJitCompilerOption("baseline.warmup.trigger", 10);
setJitCompilerOption("ion.warmup.trigger", 90);
var max = 100; // Make have the warm-up counter high enough to
               // consider inlining functions.

var f4 = SIMD.Int32x4; // :TODO: Support Float32x4 arith.
var f4add = f4.add;
var f4sub = f4.sub;
var f4mul = f4.mul;

function c4mul(z1, z2) {
  var { re: re1, im: im1 } = z1;
  var { re: re2, im: im2 } = z2;
  var rere = f4mul(re1, re2);
  var reim = f4mul(re1, im2);
  var imre = f4mul(im1, re2);
  var imim = f4mul(im1, im2);
  return { re: f4sub(rere, imim), im: f4add(reim, imre) };
}

function c4inv(z) {
  var { re: re, im: im } = z;
  var minus = f4(-1, -1, -1, -1);
  return { re: re, im: f4mul(im, minus) };
}

function c4inv_inplace(z) {
  var res = c4inv(z);
  z.re = res.re;
  z.im = res.im;
}

function c4norm(z) {
  var { re: re, im: im } = c4mul(z, c4inv(z));
  return re;
}

function c4scale(z, s) {
  var { re: re, im: im } = z;
  var f4s = f4(s, s, s, s);
  return { re: f4mul(re, f4s), im: f4mul(im, f4s) };
}

var rotate90 = { re: f4(0, 0, 0, 0), im: f4(1, 1, 1, 1) };
var cardinals = { re: f4(1, 0, -1, 0), im: f4(0, 1, 0, -1) };

function test(dots) {
  for (var j = 0; j < 4; j++) {
    dots = c4mul(rotate90, dots);
    if (j % 2 == 0) // Magic !
      c4inv_inplace(dots);
    dots = c4scale(dots, 2);
  }
  return dots;
}

assertEqX4(c4norm(cardinals), simdToArray(f4.splat(1)));
var cardinals16 = c4scale(cardinals, 16);

for (var i = 0; i < max; i++) {
  var res = test(cardinals);
  assertEqX4(c4norm(res), simdToArray(f4.splat(16 * 16)));
  assertEqX4(res.re, simdToArray(cardinals16.re));
  assertEqX4(res.im, simdToArray(cardinals16.im));
}
