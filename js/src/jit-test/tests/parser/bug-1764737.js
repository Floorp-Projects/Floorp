// |jit-test| --fuzzing-safe; --ion-offthread-compile=off

function r(src) {
  oomTest(function() {
      parseModule(src);
  });
}
r("export * from 'y';");
r("export * from 'y';");
