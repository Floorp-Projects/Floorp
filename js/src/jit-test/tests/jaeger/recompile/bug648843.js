
var HOTLOOP = 100;
function jit(on)
{
  options().match
}
function options() { return "tracejit,methodjit"; }
gczeal(2);
for (i = 0; i < HOTLOOP ; ++i) { jit(jit(42, [])); }
