// |jit-test| error: TypeError

(function() {
  var a, b;
  for each (a in [{}, {__iterator__: function(){}}]) 
    for (b in a) { }
})();

