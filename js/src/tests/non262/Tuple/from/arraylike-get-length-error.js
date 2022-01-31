// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

/* 9. Let len be ? LengthOfArrayLike(arrayLike). */

var arrayLike = {};

Object.defineProperty(arrayLike, "length", {
  get: function() {
    throw new SyntaxError();
  }
});

assertThrowsInstanceOf(function() {
  Tuple.from(arrayLike);
}, SyntaxError, "items.length throws");

reportCompare(0, 0);
