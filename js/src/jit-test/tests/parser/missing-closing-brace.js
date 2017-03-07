function test(source, [lineNumber, columnNumber]) {
  let caught = false;
  try {
    Reflect.parse(source, { source: "foo.js" });
  } catch (e) {
    assertEq(e.message.includes("missing } "), true);
    let notes = getErrorNotes(e);
    assertEq(notes.length, 1);
    let note = notes[0];
    assertEq(note.message, "{ opened at line " + lineNumber + ", column " + columnNumber);
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
`, [4, 17]);
