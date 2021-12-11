var state = 'initial';
try {
    throw new Error('caught');
    state = 'unreachable';
} catch { // Note the lack of a binding
    assertEq(state, 'initial');
    state = 'caught';
}
assertEq(state, 'caught');


var sigil1 = {};
try {
  throw sigil1;
} catch (e) {
  assertEq(e, sigil1);
}


var sigil2 = {};
var reached = false;
try {
  try {
    throw sigil1;
  } catch {
    reached = true;
  } finally {
    throw sigil2;
  }
} catch (e) {
  assertEq(e, sigil2);
}
assertEq(reached, true);

if (typeof reportCompare === "function")
    reportCompare(0, 0);
