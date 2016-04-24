
function Q(on)
{
  options().match
}
function options() { return "methodjit"; }
gczeal(2);
for (i = 0; i < 100 ; ++i) { Q(Q(42, [])); }
