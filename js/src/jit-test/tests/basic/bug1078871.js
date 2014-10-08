
// Robert Jenkins' 32 bit integer hash function
var seed = 100;
Math.random = function() {
   seed = (seed + 0x7ed55d16) + (seed<<12);
   seed = (seed ^ 0xc761c23c) ^ (seed>>19);
   seed = (seed + 0x165667b1) + (seed<<5);
   seed = (seed + 0xd3a2646c) ^ (seed<<9);
   seed = (seed + 0xfd7046c5) + (seed<<3);
   seed = (seed ^ 0xb55a4f09) ^ (seed>>16);
   seed = Math.abs(seed | 0);
   return seed / 0xffffffff * 2;
}

function tangle(n, m) {
  function rand(n) {
    return Math.floor(Math.random() * n);
  }

  var arr = [];
  for (var i = 0; i < n; i++)
    arr[i] = String.fromCharCode(65 + rand(26));
  for (var i = 0; i < m; i++) {
    var j = rand(n);
    switch (rand(2)) {
      case 0: {
        var s = arr[rand(n)];
        var b = rand(s.length);
        var e = b + rand(s.length - b);
        if (e - b > 1)
          arr[j] = s.substring(b, e);
      }
      break;
      case 1: {
        arr[j] = arr[rand(n)] + arr[rand(n)];
      }
    }
    uneval(arr[j]);
  }

  return arr;
}

tangle(10, 500);
