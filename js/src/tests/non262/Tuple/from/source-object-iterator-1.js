// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var array = [2, 4, 8, 16, 32, 64, 128];
var obj = {
  [Symbol.iterator]() {
    return {
      index: 0,
      next() {
        throw new RangeError();
      },
      isDone: false,
      get val() {
        this.index++;
        if (this.index > 7) {
          this.isDone = true;
        }
        return 1 << this.index;
      }
    };
  }
};
assertThrowsInstanceOf(function() {
  Tuple.from(obj);
}, RangeError, 'Tuple.from(obj) throws');

reportCompare(0, 0);
