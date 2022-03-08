function assertTestTrialActive(shouldBeActive) {
  is(
    !!navigator.testTrialGatedAttribute,
    shouldBeActive,
    "Should match active status for Navigator.testTrialControlledAttribute"
  );
  // FIXME(emilio): Add more tests.
  //  * Stuff hanging off Window or Document (known broken for now, we check
  //    enabledness too early).
  //  * Interfaces (unknown status, but probably same because ^).
}
