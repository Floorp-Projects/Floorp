
function cmp_string_string(a,b) {
  return a === b;
}

assertEq(cmp_string_string("a", "a"), true);
assertEq(cmp_string_string("a", "b"), false);
assertEq(cmp_string_string("a", 1), false);

function cmp_string_string2(a,b) {
  return a === b;
}

assertEq(cmp_string_string2("a", 1.1), false);
assertEq(cmp_string_string2("a", 2), false);
assertEq(cmp_string_string2("a", {}), false);

function cmp_string_string3(a,b) {
  return a !== b;
}

assertEq(cmp_string_string3("a", "a"), false);
assertEq(cmp_string_string3("a", "b"), true);
assertEq(cmp_string_string3("a", 1), true);

function cmp_string_string4(a,b) {
  return a !== b;
}

assertEq(cmp_string_string4("a", 1.1), true);
assertEq(cmp_string_string4("a", 2), true);
assertEq(cmp_string_string4("a", {}), true);
