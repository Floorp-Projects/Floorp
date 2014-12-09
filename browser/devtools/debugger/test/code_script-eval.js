
var bar;

function evalSource() {
  eval('bar = function() {\nvar x = 5;\n}');
}

function evalSourceWithSourceURL() {
  eval('bar = function() {\nvar x = 6;\n} //# sourceURL=bar.js');
}
