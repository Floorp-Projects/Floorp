// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

function potatoMasher(obj, arg) { this.eval(arg); }
potatoMasher(this, "var s = Error().stack");
assertEq(/potatoMasher/.exec(s) instanceof Array, true);

reportCompare(0, 0, 'ok');
