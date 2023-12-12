// |reftest| skip-if(!this.hasOwnProperty('Intl')||!this.Intl.Segmenter)

// Test fails in ASan builds when ubrk_refreshUText isn't called.

let string = "A. ";

let segmenter = new Intl.Segmenter(undefined, {granularity: "sentence"});
let segments = segmenter.segment(string.repeat(100));

for (let {segment} of segments) {
  assertEq(segment, string);
}

if (typeof reportCompare === "function")
  reportCompare(0, 0);
