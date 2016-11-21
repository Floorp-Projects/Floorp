// |jit-test| error: TypeError; need-for-each

(function() {
  var a, b;
  for each (a in [{}, {__iterator__: function(){}}]) 
    for (b in a) { }
})();

