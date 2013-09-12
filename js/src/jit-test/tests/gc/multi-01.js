/* Make sure we don't collect the atoms compartment unless every compartment is marked. */

var g = newGlobal();
g.eval("var x = 'some-atom';");

schedulegc(this);
schedulegc('atoms');
gc('compartment');
print(g.x);
