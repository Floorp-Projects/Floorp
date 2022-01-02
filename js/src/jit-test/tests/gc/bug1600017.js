var registry = new FinalizationRegistry(x => {
  if (target1 === null) {
      return;
  }

  target1 = null;

  gc();

  print("targets:", [...x]); // consume
});

var target1 = {};
registry.register(target1, "target1");

var target2 = {};
registry.register(target2, "target2");

target2 = null;

gc();
