gczeal(10);
x = 1;
newGlobal();
oomAtAllocation(8);
try {
  newGlobal();
} catch (e) {}
oomAtAllocation(8);
try {
  newGlobal();
} catch (e) {}
oomAtAllocation(8);
try {
  newGlobal();
} catch (e) {}
newGlobal();
