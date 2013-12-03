// |jit-test| ion-eager

// bug 944963
function bug944963(x, y) {
    (+(xy))(y % y)
}
for (var i = 0; i < 10; i++) {
    try {
        (function() {
            bug944963(0, (~Math.fround(-8)))
        })()
    } catch (e) {}
}

// bug 900437
function bug900437() {
    var x = 0.0;
    for (var i = 0; i < 10; i++)
        -("") >> (x / x);
}
bug900437();
bug900437();

// bug 715460
function f(x) {
    var a = x;
    return a / 10;
}
function g(x) {
    var y = x + 1;
    return y / y;
}
for (var i=0; i<10; i++)
    assertEq(f(i * 10), i);
for (var i=0; i<10; i++)
    assertEq(g(i), 1);

// bug 939893
function bug939893() {
    bug_g();
}
function bug_g() {
    bug_h(undefined >>> 0, +undefined);
}
function bug_h(x) {
    Math.max(x ? ((x / x) | 0) : 0);
}
for (var a = 0; a < 2; ++a) {
  bug939893();
}

// Assorted tests.

function sdiv_truncate(y) {
  return (y / y)|0;
}
assertEq(sdiv_truncate(5), 1);
assertEq(sdiv_truncate(1), 1);
assertEq(sdiv_truncate(-1), 1);
assertEq(sdiv_truncate(0), 0);
assertEq(sdiv_truncate(-0), 0);
assertEq(sdiv_truncate(1.1), 1);
assertEq(sdiv_truncate(-1.1), 1);
assertEq(sdiv_truncate(Infinity), 0);
assertEq(sdiv_truncate(NaN), 0);
assertEq(sdiv_truncate(undefined), 0);
assertEq(sdiv_truncate(null), 0);

function sdiv(y) {
  return y / y;
}
assertEq(sdiv(5), 1);
assertEq(sdiv(1), 1);
assertEq(sdiv(-1), 1);
assertEq(sdiv(0), NaN);
assertEq(sdiv(-0), NaN);
assertEq(sdiv(1.1), 1);
assertEq(sdiv(-1.1), 1);
assertEq(sdiv(Infinity), NaN);
assertEq(sdiv(NaN), NaN);
assertEq(sdiv(undefined), NaN);
assertEq(sdiv(null), NaN);

function udiv_truncate(y) {
  var yu = y>>>0;
  return (yu / yu)|0;
}
assertEq(udiv_truncate(5), 1);
assertEq(udiv_truncate(1), 1);
assertEq(udiv_truncate(-1), 1);
assertEq(udiv_truncate(0), 0);
assertEq(udiv_truncate(-0), 0);
assertEq(udiv_truncate(1.1), 1);
assertEq(udiv_truncate(-1.1), 1);
assertEq(udiv_truncate(Infinity), 0);
assertEq(udiv_truncate(NaN), 0);
assertEq(udiv_truncate(undefined), 0);
assertEq(udiv_truncate(null), 0);

function shifted_udiv_truncate(y) {
  var yu = y>>>1;
  return (yu / yu)|0;
}
assertEq(shifted_udiv_truncate(5), 1);
assertEq(shifted_udiv_truncate(2), 1);
assertEq(shifted_udiv_truncate(1), 0);
assertEq(shifted_udiv_truncate(-1), 1);
assertEq(shifted_udiv_truncate(0), 0);
assertEq(shifted_udiv_truncate(-0), 0);
assertEq(shifted_udiv_truncate(1.1), 0);
assertEq(shifted_udiv_truncate(-1.1), 1);
assertEq(shifted_udiv_truncate(Infinity), 0);
assertEq(shifted_udiv_truncate(NaN), 0);
assertEq(shifted_udiv_truncate(undefined), 0);
assertEq(shifted_udiv_truncate(null), 0);

function udiv(y) {
  var yu = y>>>0;
  return yu / yu;
}
assertEq(udiv(5), 1);
assertEq(udiv(1), 1);
assertEq(udiv(-1), 1);
assertEq(udiv(0), NaN);
assertEq(udiv(-0), NaN);
assertEq(udiv(1.1), 1);
assertEq(udiv(-1.1), 1);
assertEq(udiv(Infinity), NaN);
assertEq(udiv(NaN), NaN);
assertEq(udiv(undefined), NaN);
assertEq(udiv(null), NaN);

function shifted_udiv(y) {
  var yu = y>>>1;
  return yu / yu;
}
assertEq(shifted_udiv(5), 1);
assertEq(shifted_udiv(2), 1);
assertEq(shifted_udiv(1), NaN);
assertEq(shifted_udiv(-1), 1);
assertEq(shifted_udiv(0), NaN);
assertEq(shifted_udiv(-0), NaN);
assertEq(shifted_udiv(1.1), NaN);
assertEq(shifted_udiv(-1.1), 1);
assertEq(shifted_udiv(Infinity), NaN);
assertEq(shifted_udiv(NaN), NaN);
assertEq(shifted_udiv(undefined), NaN);
assertEq(shifted_udiv(null), NaN);

function smod_truncate(y) {
  return (y % y)|0;
}
assertEq(smod_truncate(5), 0);
assertEq(smod_truncate(1), 0);
assertEq(smod_truncate(-1), 0);
assertEq(smod_truncate(0), 0);
assertEq(smod_truncate(-0), 0);
assertEq(smod_truncate(1.1), 0);
assertEq(smod_truncate(-1.1), 0);
assertEq(smod_truncate(Infinity), 0);
assertEq(smod_truncate(NaN), 0);
assertEq(smod_truncate(undefined), 0);
assertEq(smod_truncate(null), 0);

function smod(y) {
  return y % y;
}
assertEq(smod(5), 0);
assertEq(smod(1), 0);
assertEq(smod(-1), -0);
assertEq(smod(0), NaN);
assertEq(smod(-0), NaN);
assertEq(smod(1.1), 0);
assertEq(smod(-1.1), -0);
assertEq(smod(Infinity), NaN);
assertEq(smod(NaN), NaN);
assertEq(smod(undefined), NaN);
assertEq(smod(null), NaN);

function umod_truncate(y) {
  var yu = y>>>0;
  return (yu % yu)|0;
}
assertEq(umod_truncate(5), 0);
assertEq(umod_truncate(1), 0);
assertEq(umod_truncate(-1), 0);
assertEq(umod_truncate(0), 0);
assertEq(umod_truncate(-0), 0);
assertEq(umod_truncate(1.1), 0);
assertEq(umod_truncate(-1.1), 0);
assertEq(umod_truncate(Infinity), 0);
assertEq(umod_truncate(NaN), 0);
assertEq(umod_truncate(undefined), 0);
assertEq(umod_truncate(null), 0);

function shifted_umod_truncate(y) {
  var yu = y>>>1;
  return (yu % yu)|0;
}
assertEq(shifted_umod_truncate(5), 0);
assertEq(shifted_umod_truncate(2), 0);
assertEq(shifted_umod_truncate(1), 0);
assertEq(shifted_umod_truncate(-1), 0);
assertEq(shifted_umod_truncate(0), 0);
assertEq(shifted_umod_truncate(-0), 0);
assertEq(shifted_umod_truncate(1.1), 0);
assertEq(shifted_umod_truncate(-1.1), 0);
assertEq(shifted_umod_truncate(Infinity), 0);
assertEq(shifted_umod_truncate(NaN), 0);
assertEq(shifted_umod_truncate(undefined), 0);
assertEq(shifted_umod_truncate(null), 0);

function umod(y) {
  var yu = y>>>0;
  return yu % yu;
}
assertEq(umod(5), 0);
assertEq(umod(1), 0);
assertEq(umod(-1), 0);
assertEq(umod(0), NaN);
assertEq(umod(-0), NaN);
assertEq(umod(1.1), 0);
assertEq(umod(-1.1), 0);
assertEq(umod(Infinity), NaN);
assertEq(umod(NaN), NaN);
assertEq(umod(undefined), NaN);
assertEq(umod(null), NaN);

function shifted_umod(y) {
  var yu = y>>>1;
  return yu % yu;
}
assertEq(shifted_umod(5), 0);
assertEq(shifted_umod(2), 0);
assertEq(shifted_umod(1), NaN);
assertEq(shifted_umod(-1), 0);
assertEq(shifted_umod(0), NaN);
assertEq(shifted_umod(-0), NaN);
assertEq(shifted_umod(1.1), NaN);
assertEq(shifted_umod(-1.1), 0);
assertEq(shifted_umod(Infinity), NaN);
assertEq(shifted_umod(NaN), NaN);
assertEq(shifted_umod(undefined), NaN);
assertEq(shifted_umod(null), NaN);

function sdiv_truncate_nonzero(y) {
  if (y == 0) return -202;
  return (y / y)|0;
}
assertEq(sdiv_truncate_nonzero(5), 1);
assertEq(sdiv_truncate_nonzero(1), 1);
assertEq(sdiv_truncate_nonzero(-1), 1);
assertEq(sdiv_truncate_nonzero(0), -202);
assertEq(sdiv_truncate_nonzero(-0), -202);
assertEq(sdiv_truncate_nonzero(1.1), 1);
assertEq(sdiv_truncate_nonzero(-1.1), 1);
assertEq(sdiv_truncate_nonzero(Infinity), 0);
assertEq(sdiv_truncate_nonzero(NaN), 0);
assertEq(sdiv_truncate_nonzero(undefined), 0);
assertEq(sdiv_truncate_nonzero(null), 0);

function sdiv_nonzero(y) {
  if (y == 0) return -202;
  return y / y;
}
assertEq(sdiv_nonzero(5), 1);
assertEq(sdiv_nonzero(1), 1);
assertEq(sdiv_nonzero(-1), 1);
assertEq(sdiv_nonzero(0), -202);
assertEq(sdiv_nonzero(-0), -202);
assertEq(sdiv_nonzero(1.1), 1);
assertEq(sdiv_nonzero(-1.1), 1);
assertEq(sdiv_nonzero(Infinity), NaN);
assertEq(sdiv_nonzero(NaN), NaN);
assertEq(sdiv_nonzero(undefined), NaN);
assertEq(sdiv_nonzero(null), NaN);

function udiv_truncate_nonzero(y) {
  var yu = y>>>0;
  if (yu == 0) return -202;
  return (yu / yu)|0;
}
assertEq(udiv_truncate_nonzero(5), 1);
assertEq(udiv_truncate_nonzero(1), 1);
assertEq(udiv_truncate_nonzero(-1), 1);
assertEq(udiv_truncate_nonzero(0), -202);
assertEq(udiv_truncate_nonzero(-0), -202);
assertEq(udiv_truncate_nonzero(1.1), 1);
assertEq(udiv_truncate_nonzero(-1.1), 1);
assertEq(udiv_truncate_nonzero(Infinity), -202);
assertEq(udiv_truncate_nonzero(NaN), -202);
assertEq(udiv_truncate_nonzero(undefined), -202);
assertEq(udiv_truncate_nonzero(null), -202);

function shifted_udiv_truncate_nonzero(y) {
  var yu = y>>>1;
  if (yu == 0) return -202;
  return (yu / yu)|0;
}
assertEq(shifted_udiv_truncate_nonzero(5), 1);
assertEq(shifted_udiv_truncate_nonzero(2), 1);
assertEq(shifted_udiv_truncate_nonzero(1), -202);
assertEq(shifted_udiv_truncate_nonzero(-1), 1);
assertEq(shifted_udiv_truncate_nonzero(0), -202);
assertEq(shifted_udiv_truncate_nonzero(-0), -202);
assertEq(shifted_udiv_truncate_nonzero(1.1), -202);
assertEq(shifted_udiv_truncate_nonzero(-1.1), 1);
assertEq(shifted_udiv_truncate_nonzero(Infinity), -202);
assertEq(shifted_udiv_truncate_nonzero(NaN), -202);
assertEq(shifted_udiv_truncate_nonzero(undefined), -202);
assertEq(shifted_udiv_truncate_nonzero(null), -202);

function udiv_nonzero(y) {
  var yu = y>>>0;
  if (yu == 0) return -202;
  return yu / yu;
}
assertEq(udiv_nonzero(5), 1);
assertEq(udiv_nonzero(1), 1);
assertEq(udiv_nonzero(-1), 1);
assertEq(udiv_nonzero(0), -202);
assertEq(udiv_nonzero(-0), -202);
assertEq(udiv_nonzero(1.1), 1);
assertEq(udiv_nonzero(-1.1), 1);
assertEq(udiv_nonzero(Infinity), -202);
assertEq(udiv_nonzero(NaN), -202);
assertEq(udiv_nonzero(undefined), -202);
assertEq(udiv_nonzero(null), -202);

function shifted_udiv_nonzero(y) {
  var yu = y>>>1;
  if (yu == 0) return -202;
  return yu / yu;
}
assertEq(shifted_udiv_nonzero(5), 1);
assertEq(shifted_udiv_nonzero(2), 1);
assertEq(shifted_udiv_nonzero(1), -202);
assertEq(shifted_udiv_nonzero(-1), 1);
assertEq(shifted_udiv_nonzero(0), -202);
assertEq(shifted_udiv_nonzero(-0), -202);
assertEq(shifted_udiv_nonzero(1.1), -202);
assertEq(shifted_udiv_nonzero(-1.1), 1);
assertEq(shifted_udiv_nonzero(Infinity), -202);
assertEq(shifted_udiv_nonzero(NaN), -202);
assertEq(shifted_udiv_nonzero(undefined), -202);
assertEq(shifted_udiv_nonzero(null), -202);

function smod_truncate_nonzero(y) {
  if (y == 0) return -202;
  return (y % y)|0;
}
assertEq(smod_truncate_nonzero(5), 0);
assertEq(smod_truncate_nonzero(1), 0);
assertEq(smod_truncate_nonzero(-1), 0);
assertEq(smod_truncate_nonzero(0), -202);
assertEq(smod_truncate_nonzero(-0), -202);
assertEq(smod_truncate_nonzero(1.1), 0);
assertEq(smod_truncate_nonzero(-1.1), 0);
assertEq(smod_truncate_nonzero(Infinity), 0);
assertEq(smod_truncate_nonzero(NaN), 0);
assertEq(smod_truncate_nonzero(undefined), 0);
assertEq(smod_truncate_nonzero(null), 0);

function smod_nonzero(y) {
  if (y == 0) return -202;
  return y % y;
}
assertEq(smod_nonzero(5), 0);
assertEq(smod_nonzero(1), 0);
assertEq(smod_nonzero(-1), -0);
assertEq(smod_nonzero(0), -202);
assertEq(smod_nonzero(-0), -202);
assertEq(smod_nonzero(1.1), 0);
assertEq(smod_nonzero(-1.1), -0);
assertEq(smod_nonzero(Infinity), NaN);
assertEq(smod_nonzero(NaN), NaN);
assertEq(smod_nonzero(undefined), NaN);
assertEq(smod_nonzero(null), NaN);

function umod_truncate_nonzero(y) {
  var yu = y>>>0;
  if (yu == 0) return -202;
  return (yu % yu)|0;
}
assertEq(umod_truncate_nonzero(5), 0);
assertEq(umod_truncate_nonzero(1), 0);
assertEq(umod_truncate_nonzero(-1), 0);
assertEq(umod_truncate_nonzero(0), -202);
assertEq(umod_truncate_nonzero(-0), -202);
assertEq(umod_truncate_nonzero(1.1), 0);
assertEq(umod_truncate_nonzero(-1.1), 0);
assertEq(umod_truncate_nonzero(Infinity), -202);
assertEq(umod_truncate_nonzero(NaN), -202);
assertEq(umod_truncate_nonzero(undefined), -202);
assertEq(umod_truncate_nonzero(null), -202);

function shifted_umod_truncate_nonzero(y) {
  var yu = y>>>1;
  if (yu == 0) return -202;
  return (yu % yu)|0;
}
assertEq(shifted_umod_truncate_nonzero(5), 0);
assertEq(shifted_umod_truncate_nonzero(2), 0);
assertEq(shifted_umod_truncate_nonzero(1), -202);
assertEq(shifted_umod_truncate_nonzero(-1), 0);
assertEq(shifted_umod_truncate_nonzero(0), -202);
assertEq(shifted_umod_truncate_nonzero(-0), -202);
assertEq(shifted_umod_truncate_nonzero(1.1), -202);
assertEq(shifted_umod_truncate_nonzero(-1.1), 0);
assertEq(shifted_umod_truncate_nonzero(Infinity), -202);
assertEq(shifted_umod_truncate_nonzero(NaN), -202);
assertEq(shifted_umod_truncate_nonzero(undefined), -202);
assertEq(shifted_umod_truncate_nonzero(null), -202);

function umod_nonzero(y) {
  var yu = y>>>0;
  if (yu == 0) return -202;
  return yu % yu;
}
assertEq(umod_nonzero(5), 0);
assertEq(umod_nonzero(1), 0);
assertEq(umod_nonzero(-1), 0);
assertEq(umod_nonzero(0), -202);
assertEq(umod_nonzero(-0), -202);
assertEq(umod_nonzero(1.1), 0);
assertEq(umod_nonzero(-1.1), 0);
assertEq(umod_nonzero(Infinity), -202);
assertEq(umod_nonzero(NaN), -202);
assertEq(umod_nonzero(undefined), -202);
assertEq(umod_nonzero(null), -202);

function shifted_umod_nonzero(y) {
  var yu = y>>>1;
  if (yu == 0) return -202;
  return yu % yu;
}
assertEq(shifted_umod_nonzero(5), 0);
assertEq(shifted_umod_nonzero(2), 0);
assertEq(shifted_umod_nonzero(1), -202);
assertEq(shifted_umod_nonzero(-1), 0);
assertEq(shifted_umod_nonzero(0), -202);
assertEq(shifted_umod_nonzero(-0), -202);
assertEq(shifted_umod_nonzero(1.1), -202);
assertEq(shifted_umod_nonzero(-1.1), 0);
assertEq(shifted_umod_nonzero(Infinity), -202);
assertEq(shifted_umod_nonzero(NaN), -202);
assertEq(shifted_umod_nonzero(undefined), -202);
assertEq(shifted_umod_nonzero(null), -202);

function sdiv_truncate_positive(y) {
  if (y <= 0) return -202;
  return (y / y)|0;
}
assertEq(sdiv_truncate_positive(5), 1);
assertEq(sdiv_truncate_positive(1), 1);
assertEq(sdiv_truncate_positive(-1), -202);
assertEq(sdiv_truncate_positive(0), -202);
assertEq(sdiv_truncate_positive(-0), -202);
assertEq(sdiv_truncate_positive(1.1), 1);
assertEq(sdiv_truncate_positive(-1.1), -202);
assertEq(sdiv_truncate_positive(Infinity), 0);
assertEq(sdiv_truncate_positive(NaN), 0);
assertEq(sdiv_truncate_positive(undefined), 0);
assertEq(sdiv_truncate_positive(null), -202);

function sdiv_positive(y) {
  if (y <= 0) return -202;
  return y / y;
}
assertEq(sdiv_positive(5), 1);
assertEq(sdiv_positive(1), 1);
assertEq(sdiv_positive(-1), -202);
assertEq(sdiv_positive(0), -202);
assertEq(sdiv_positive(-0), -202);
assertEq(sdiv_positive(1.1), 1);
assertEq(sdiv_positive(-1.1), -202);
assertEq(sdiv_positive(Infinity), NaN);
assertEq(sdiv_positive(NaN), NaN);
assertEq(sdiv_positive(undefined), NaN);
assertEq(sdiv_positive(null), -202);

function udiv_truncate_positive(y) {
  var yu = y>>>0;
  if (yu <= 0) return -202;
  return (yu / yu)|0;
}
assertEq(udiv_truncate_positive(5), 1);
assertEq(udiv_truncate_positive(1), 1);
assertEq(udiv_truncate_positive(-1), 1);
assertEq(udiv_truncate_positive(0), -202);
assertEq(udiv_truncate_positive(-0), -202);
assertEq(udiv_truncate_positive(1.1), 1);
assertEq(udiv_truncate_positive(-1.1), 1);
assertEq(udiv_truncate_positive(Infinity), -202);
assertEq(udiv_truncate_positive(NaN), -202);
assertEq(udiv_truncate_positive(undefined), -202);
assertEq(udiv_truncate_positive(null), -202);

function shifted_udiv_truncate_positive(y) {
  var yu = y>>>1;
  if (yu <= 0) return -202;
  return (yu / yu)|0;
}
assertEq(shifted_udiv_truncate_positive(5), 1);
assertEq(shifted_udiv_truncate_positive(2), 1);
assertEq(shifted_udiv_truncate_positive(1), -202);
assertEq(shifted_udiv_truncate_positive(-1), 1);
assertEq(shifted_udiv_truncate_positive(0), -202);
assertEq(shifted_udiv_truncate_positive(-0), -202);
assertEq(shifted_udiv_truncate_positive(1.1), -202);
assertEq(shifted_udiv_truncate_positive(-1.1), 1);
assertEq(shifted_udiv_truncate_positive(Infinity), -202);
assertEq(shifted_udiv_truncate_positive(NaN), -202);
assertEq(shifted_udiv_truncate_positive(undefined), -202);
assertEq(shifted_udiv_truncate_positive(null), -202);

function udiv_positive(y) {
  var yu = y>>>0;
  if (yu <= 0) return -202;
  return yu / yu;
}
assertEq(udiv_positive(5), 1);
assertEq(udiv_positive(1), 1);
assertEq(udiv_positive(-1), 1);
assertEq(udiv_positive(0), -202);
assertEq(udiv_positive(-0), -202);
assertEq(udiv_positive(1.1), 1);
assertEq(udiv_positive(-1.1), 1);
assertEq(udiv_positive(Infinity), -202);
assertEq(udiv_positive(NaN), -202);
assertEq(udiv_positive(undefined), -202);
assertEq(udiv_positive(null), -202);

function shifted_udiv_positive(y) {
  var yu = y>>>1;
  if (yu <= 0) return -202;
  return yu / yu;
}
assertEq(shifted_udiv_positive(5), 1);
assertEq(shifted_udiv_positive(2), 1);
assertEq(shifted_udiv_positive(1), -202);
assertEq(shifted_udiv_positive(-1), 1);
assertEq(shifted_udiv_positive(0), -202);
assertEq(shifted_udiv_positive(-0), -202);
assertEq(shifted_udiv_positive(1.1), -202);
assertEq(shifted_udiv_positive(-1.1), 1);
assertEq(shifted_udiv_positive(Infinity), -202);
assertEq(shifted_udiv_positive(NaN), -202);
assertEq(shifted_udiv_positive(undefined), -202);
assertEq(shifted_udiv_positive(null), -202);

function smod_truncate_positive(y) {
  if (y <= 0) return -202;
  return (y % y)|0;
}
assertEq(smod_truncate_positive(5), 0);
assertEq(smod_truncate_positive(1), 0);
assertEq(smod_truncate_positive(-1), -202);
assertEq(smod_truncate_positive(0), -202);
assertEq(smod_truncate_positive(-0), -202);
assertEq(smod_truncate_positive(1.1), 0);
assertEq(smod_truncate_positive(-1.1), -202);
assertEq(smod_truncate_positive(Infinity), 0);
assertEq(smod_truncate_positive(NaN), 0);
assertEq(smod_truncate_positive(undefined), 0);
assertEq(smod_truncate_positive(null), -202);

function smod_positive(y) {
  if (y <= 0) return -202;
  return y % y;
}
assertEq(smod_positive(5), 0);
assertEq(smod_positive(1), 0);
assertEq(smod_positive(-1), -202);
assertEq(smod_positive(0), -202);
assertEq(smod_positive(-0), -202);
assertEq(smod_positive(1.1), 0);
assertEq(smod_positive(-1.1), -202);
assertEq(smod_positive(Infinity), NaN);
assertEq(smod_positive(NaN), NaN);
assertEq(smod_positive(undefined), NaN);
assertEq(smod_positive(null), -202);

function umod_truncate_positive(y) {
  var yu = y>>>0;
  if (yu <= 0) return -202;
  return (yu % yu)|0;
}
assertEq(umod_truncate_positive(5), 0);
assertEq(umod_truncate_positive(1), 0);
assertEq(umod_truncate_positive(-1), 0);
assertEq(umod_truncate_positive(0), -202);
assertEq(umod_truncate_positive(-0), -202);
assertEq(umod_truncate_positive(1.1), 0);
assertEq(umod_truncate_positive(-1.1), 0);
assertEq(umod_truncate_positive(Infinity), -202);
assertEq(umod_truncate_positive(NaN), -202);
assertEq(umod_truncate_positive(undefined), -202);
assertEq(umod_truncate_positive(null), -202);

function shifted_umod_truncate_positive(y) {
  var yu = y>>>1;
  if (yu <= 0) return -202;
  return (yu % yu)|0;
}
assertEq(shifted_umod_truncate_positive(5), 0);
assertEq(shifted_umod_truncate_positive(2), 0);
assertEq(shifted_umod_truncate_positive(1), -202);
assertEq(shifted_umod_truncate_positive(-1), 0);
assertEq(shifted_umod_truncate_positive(0), -202);
assertEq(shifted_umod_truncate_positive(-0), -202);
assertEq(shifted_umod_truncate_positive(1.1), -202);
assertEq(shifted_umod_truncate_positive(-1.1), 0);
assertEq(shifted_umod_truncate_positive(Infinity), -202);
assertEq(shifted_umod_truncate_positive(NaN), -202);
assertEq(shifted_umod_truncate_positive(undefined), -202);
assertEq(shifted_umod_truncate_positive(null), -202);

function umod_positive(y) {
  var yu = y>>>0;
  if (yu <= 0) return -202;
  return yu % yu;
}
assertEq(umod_positive(5), 0);
assertEq(umod_positive(1), 0);
assertEq(umod_positive(-1), 0);
assertEq(umod_positive(0), -202);
assertEq(umod_positive(-0), -202);
assertEq(umod_positive(1.1), 0);
assertEq(umod_positive(-1.1), 0);
assertEq(umod_positive(Infinity), -202);
assertEq(umod_positive(NaN), -202);
assertEq(umod_positive(undefined), -202);
assertEq(umod_positive(null), -202);

function shifted_umod_positive(y) {
  var yu = y>>>1;
  if (yu <= 0) return -202;
  return yu % yu;
}
assertEq(shifted_umod_positive(5), 0);
assertEq(shifted_umod_positive(2), 0);
assertEq(shifted_umod_positive(1), -202);
assertEq(shifted_umod_positive(-1), 0);
assertEq(shifted_umod_positive(0), -202);
assertEq(shifted_umod_positive(-0), -202);
assertEq(shifted_umod_positive(1.1), -202);
assertEq(shifted_umod_positive(-1.1), 0);
assertEq(shifted_umod_positive(Infinity), -202);
assertEq(shifted_umod_positive(NaN), -202);
assertEq(shifted_umod_positive(undefined), -202);
assertEq(shifted_umod_positive(null), -202);
