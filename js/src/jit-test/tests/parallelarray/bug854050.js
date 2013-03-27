function bug854050() {
  // Shouldn't crash. Tests Ion's argumentsRectifier loading the right
  // IonScript depending on execution mode.
  for (z = 0; z < 1; z++)
    function x(b, x) {}
  ParallelArray(47, x);
}

bug854050();
