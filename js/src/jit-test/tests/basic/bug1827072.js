assertThrows(() => newString("", { capacity: 1 }));
assertThrows(() => newString("x", { capacity: 2 }));

// Too large for an inline string.
const nonInlineLinear = "123456789012345678901234567890";
assertEq(nonInlineLinear.length, 30);

newString(nonInlineLinear, { capacity: 29, tenured: true });
newString(nonInlineLinear, { capacity: 30, tenured: true });
newString(nonInlineLinear, { capacity: 31, tenured: true });

function assertThrows(f) {
  try {
    f();
  } catch {
    return;
  }
  throw new Error("missing error");
}
