function arraysEqual(a, b) {
  if (a.length !== b.length) {
    return false;
  }
  for (let i = 0; i < b.length; i++) {
    if (a[i] !== b[i]) {
      return false;
    }
  }
  return true;
}

function testCustomSection(moduleText, customSectionName, expectedBytes) {
  let module = new WebAssembly.Module(wasmTextToBinary(moduleText));

  let sections = WebAssembly.Module.customSections(module, customSectionName);
  assertEq(sections.length, 1);

  let section = sections[0];
  let sectionView = new Uint8Array(section);

  if (!arraysEqual(sectionView, expectedBytes)) {
    let got = JSON.stringify(Array.from(sectionView));
    let expected = JSON.stringify(Array.from(expectedBytes));
    assertEq(true, false, `got: ${got}, expected: ${expected}`)
  }
}

// Test an unknown custom section
testCustomSection(
  `(module (@custom "unknown" "\\00\\01\\02\\03\\04"))`,
  "unknown",
  [0, 1, 2, 3, 4]);

// Test the name section
testCustomSection(
  `(module (func $test))`,
  "name",
  [
    /* subsection is functions */ 1,
    /* subsection is 7 bytes */ 7,
    /* name map has 1 entry */ 1,
    /* first entry is index */ 0,
    /* first entry string 4 bytes */ 4,
    /* 't' */ 116,
    /* 'e' */ 101,
    /* 's' */ 115,
    /* 't' */ 116]);
