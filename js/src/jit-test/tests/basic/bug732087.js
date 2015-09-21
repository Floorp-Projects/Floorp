enableNoSuchMethod();
gczeal(2,1);
var count = 0;
var a = {__noSuchMethod__: function() { count++; } }
for (var i = 0; i < 10; i++) {
  a.b();
}
