// |jit-test| --enable-weak-refs

var group = new FinalizationGroup(x => {
  if (target1 === null) {
      return;
  }

  target1 = null;

  gc();

  print("targets:", [...x]); // consume
});

var target1 = {};
group.register(target1, "target1");

var target2 = {};
group.register(target2, "target2");

target2 = null;

gc();
