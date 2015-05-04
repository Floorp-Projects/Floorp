var path = '';

// trigger off-main-thread compilation
for (var i = 0; i < 11; i++)
  path.substr(-1);

// maybe link to the the result of the off-main-thread compilation.
function load(unsigned) {
  if (unsigned)
    path.substr(-1);
}

(function(global, env) {
  'use asm';
  var load = env.load;
  function _main() {
    var $l1 = 0, $l2 = 0, $l3 = 0;
    do {
      load();
      $l1 = $l1 + 1 | 0;
    } while (($l1 | 0) != 10);
    load(1);
    load(1);
    do {
      load();
      $l2 = $l2 + 1 | 0;
    } while (($l2 | 0) != 1024);
    while (($l3 | 0) < 10000) {
      load(1);
      $l3 = $l3 + 1 | 0;
    }
  }
  return _main;
})({}, { 'load':load })();
