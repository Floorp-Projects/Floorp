// |reftest| skip-if(!Intl.hasOwnProperty('Segmenter')) -- Intl.Segmenter is not enabled unconditionally
// Copyright 2018 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-segment-iterator-prototype
description: Verifies the behavior for the iterators.
features: [Intl.Segmenter]
---*/

const text = "Hello World, Test 123! Foo Bar. How are you?";
for (const granularity of ["grapheme", "word", "sentence", "line"]) {
  const segmenter = new Intl.Segmenter("en", { granularity });
  const iter = segmenter.segment(text);

  assert.sameValue(typeof iter.position, "number");
  assert.sameValue(iter.position, 0);
  if (granularity === "grapheme") {
    assert.sameValue(iter.breakType, undefined);
  } else {
    assert.sameValue(typeof iter.breakType, "string");
  }
}

reportCompare(0, 0);
