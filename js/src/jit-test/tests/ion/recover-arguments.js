setJitCompilerOption("baseline.warmup.trigger", 9);
setJitCompilerOption("ion.warmup.trigger", 20);

// Prevent the GC from cancelling compilations, when we expect them to succeed.
gczeal(0);

function rcreate_arguments_object_nouse() {
    assertRecoveredOnBailout(arguments, true);
}

function rcreate_arguments_object_oneuse() {
  assertRecoveredOnBailout(arguments, true);
  return arguments[0];
}

function rcreate_arguments_object_oneuse_oob() {
  assertRecoveredOnBailout(arguments, true);
  return arguments[100];
}

with ({}) {}
for (var i = 0; i < 100; i++) {
    rcreate_arguments_object_nouse();
    rcreate_arguments_object_oneuse(0);
    rcreate_arguments_object_oneuse_oob(0);
}
