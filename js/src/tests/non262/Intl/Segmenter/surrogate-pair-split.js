// |reftest| skip-if(!this.hasOwnProperty('Intl')||!this.Intl.Segmenter)

// Calling %Segments.prototype%.containing in the middle of a surrogate pair
// doubles back to the lead surrogate.

// Grapheme
{
  let segmenter = new Intl.Segmenter(undefined, {granularity: "grapheme"});

  let string = "\u{1F925}";
  let segments = segmenter.segment(string);

  let data1 = segments.containing(0);
  let data2 = segments.containing(1);
  let data3 = segments.containing(2);

  assertEq(data1.segment, string);
  assertDeepEq(data1, data2);
  assertEq(data3, undefined);
}

// Word
{
  let segmenter = new Intl.Segmenter(undefined, {granularity: "word"});

  let prefix = "Nothing to see here! ";
  let string = "\u{1F925}";
  let segments = segmenter.segment(prefix + string);

  let data1 = segments.containing(prefix.length + 0);
  let data2 = segments.containing(prefix.length + 1);
  let data3 = segments.containing(prefix.length + 2);

  assertEq(data1.segment, string);
  assertDeepEq(data1, data2);
  assertEq(data3, undefined);
}

// Sentence
{
  let segmenter = new Intl.Segmenter(undefined, {granularity: "sentence"});

  let prefix = "Nothing to see here! Please disperse. ";
  let string = "\u{1F925}";
  let segments = segmenter.segment(prefix + string);

  let data1 = segments.containing(prefix.length + 0);
  let data2 = segments.containing(prefix.length + 1);
  let data3 = segments.containing(prefix.length + 2);

  assertEq(data1.segment, string);
  assertDeepEq(data1, data2);
  assertEq(data3, undefined);
}

if (typeof reportCompare === "function")
  reportCompare(0, 0);
