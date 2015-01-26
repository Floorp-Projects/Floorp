setJitCompilerOption("baseline.warmup.trigger", 10);
setJitCompilerOption("ion.warmup.trigger", 30);
var i;

// Check that an entry frame is always aligned properly.
function entryFrame_1() {
  assertJitStackInvariants();
}

// Check rectifier frames are keeping the same alignment.
function rectifierFrame_verify(a, b, c, d) {
  assertJitStackInvariants();
}

function rectifierFrame_1(i) {
  rectifierFrame_verify();
}
function rectifierFrame_2(i) {
  rectifierFrame_verify(i);
}
function rectifierFrame_3(i) {
  rectifierFrame_verify(i, i);
}
function rectifierFrame_4(i) {
  rectifierFrame_verify(i, i, i);
}

for (i = 0; i < 40; i++) {
  entryFrame_1();
  entryFrame_1(0);
  entryFrame_1(0, 1);
  rectifierFrame_1(i);
  rectifierFrame_2(i);
  rectifierFrame_3(i);
  rectifierFrame_4(i);
}
