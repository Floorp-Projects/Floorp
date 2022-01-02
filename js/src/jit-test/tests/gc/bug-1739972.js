enableShellAllocationMetadataBuilder();
a = newGlobal();
a.evaluate("function x() {}");
for (i = 0; i < 20; ++i)
  new a.x;
