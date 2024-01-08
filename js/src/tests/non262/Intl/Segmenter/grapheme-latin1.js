// |reftest| slow skip-if(!this.hasOwnProperty('Intl')||!this.Intl.Segmenter)

// CRLF should be the only compound grapheme for Latin-1 strings.

let segmenter = new Intl.Segmenter("en", {granularity: "grapheme"});

for (let i = 0; i <= 0xff; ++i) {
  for (let j = 0; j <= 0xff; ++j) {
    let string = String.fromCodePoint(i, j);
    let segments = segmenter.segment(string);

    let data1 = segments.containing(0);
    let data2 = segments.containing(1);
    let graphemes = [...segments];

    if (i === "\r".charCodeAt(0) && j === "\n".charCodeAt(0)) {
      assertEq(data1.index, 0);
      assertEq(data1.segment, "\r\n");

      assertEq(data2.index, 0);
      assertEq(data2.segment, "\r\n");

      assertEq(graphemes.length, 1);
    } else {
      assertEq(data1.index, 0);
      assertEq(data1.segment, String.fromCodePoint(i));

      assertEq(data2.index, 1);
      assertEq(data2.segment, String.fromCodePoint(j));

      assertEq(graphemes.length, 2);
    }
  }
}

if (typeof reportCompare === "function")
  reportCompare(0, 0);
