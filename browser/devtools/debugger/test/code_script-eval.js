
var bar;

function evalSource() {
  eval('bar = function() {\nvar x = 5;\n}');
}
