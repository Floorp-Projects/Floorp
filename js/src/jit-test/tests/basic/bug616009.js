function run() {
  var obj = {
    toJSON: function() {
      return {
        key: {
          toJSON: function() {
            for (i=0; i!=1<<10; ++i)
              new Object();
            var big = unescape("%udddd");
            while (big.length != 0x100000)
               big += big;
            for (i=0; i!=32; ++i)
              new String(big+i);
            return "whatever";
          }
        },
        __iterator__: function() {
          return {
            next: function() {
              return "key";
            }
          }
        }
      }
    }
  };

  var repl = function(id, val) {
    this[0]++;
    return val;
  };

  JSON.stringify(obj, repl);
}
run();
