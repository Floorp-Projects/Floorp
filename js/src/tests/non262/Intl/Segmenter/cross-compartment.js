// |reftest| skip-if(!this.hasOwnProperty('Intl')||!this.Intl.Segmenter)

var g = newGlobal({});

var segmenter = new Intl.Segmenter();
var ccwSegmenter = new g.Intl.Segmenter();

const SegmentsPrototype = Object.getPrototypeOf(segmenter.segment(""));
const SegmentIteratorPrototype = Object.getPrototypeOf(segmenter.segment("")[Symbol.iterator]());

// Intl.Segmenter.prototype.resolvedOptions ()
var resolved1 = Intl.Segmenter.prototype.resolvedOptions.call(segmenter);
var resolved2 = Intl.Segmenter.prototype.resolvedOptions.call(ccwSegmenter);
assertDeepEq(resolved1, resolved2);

// Intl.Segmenter.prototype.segment
var seg1 = Intl.Segmenter.prototype.segment.call(segmenter, "This is a test.");
var seg2 = Intl.Segmenter.prototype.segment.call(ccwSegmenter, "This is a test.");

// %Segments.prototype%.containing ( index )
var data1 = SegmentsPrototype.containing.call(seg1, 10);
var data2 = SegmentsPrototype.containing.call(seg2, 10);
assertDeepEq(data1, data2);

// %Segments.prototype% [ @@iterator ] ()
var iter1 = SegmentsPrototype[Symbol.iterator].call(seg1);
var iter2 = SegmentsPrototype[Symbol.iterator].call(seg2);

// %SegmentIterator.prototype%.next ()
var result1 = SegmentIteratorPrototype.next.call(iter1);
var result2 = SegmentIteratorPrototype.next.call(iter2);
assertDeepEq(result1, result2);

if (typeof reportCompare === "function")
  reportCompare(0, 0);
