function test() {
  for (var i = 0; i < 10; i++)
    /0|[1-9][0-9]*/.test("");
};
test();
test();
gczeal(4);
enableShellAllocationMetadataBuilder();
