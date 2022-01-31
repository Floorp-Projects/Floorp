// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

/* 9. Let len be ? LengthOfArrayLike(arrayLike). */

var arrayLike = { length: {} };

arrayLike.length = {
  valueOf: function() {
    throw new SyntaxError();
  }
};

assertThrowsInstanceOf(() => Tuple.from(arrayLike),
                       SyntaxError, "items has invalid length");

reportCompare(0, 0);

