function test(source, [lineNumber, columnNumber], openType = "{", closeType = "}") {
  let caught = false;
  try {
    Reflect.parse(source, { source: "foo.js" });
  } catch (e) {
    assertEq(e.message.includes("missing " + closeType + " "), true);
    let notes = getErrorNotes(e);
    assertEq(notes.length, 1);
    let note = notes[0];
    assertEq(note.message, openType + " opened at line " + lineNumber + ", column " + columnNumber);
    assertEq(note.fileName, "foo.js");
    assertEq(note.lineNumber, lineNumber);
    assertEq(note.columnNumber, columnNumber);
    caught = true;
  }
  assertEq(caught, true);
}

// Function

test(`
function test1() {
}
function test2() {
  if (true) {
  //}
}
function test3() {
}
`, [4, 18]);

// Block statement.
test(`
{
  if (true) {
}
`, [2, 1]);
test(`
if (true) {
  if (true) {
}
`, [2, 11]);
test(`
for (;;) {
  if (true) {
}
`, [2, 10]);
test(`
while (true) {
  if (true) {
}
`, [2, 14]);
test(`
do {
  do {
} while(true);
`, [2, 4]);

// try-catch-finally.
test(`
try {
  if (true) {
}
`, [2, 5]);
test(`
try {
} catch (e) {
  if (true) {
}
`, [3, 13]);
test(`
try {
} finally {
  if (true) {
}
`, [3, 11]);

// Object literal.
test(`
var x = {
  foo: {
};
`, [2, 9]);

// Array literal.
test(`
var x = [
  [
];
`, [2, 9], "[", "]");
