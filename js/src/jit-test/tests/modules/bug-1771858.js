// |jit-test| exitstatus: 6;

b = parseModule("await 10");
b.declarationInstantiation();
b.evaluation();
setInterruptCallback(function() {
  c();
});
function c() {
  interruptIf(true);
}
c();
