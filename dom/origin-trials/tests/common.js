function assertTestTrialActive(shouldBeActive) {
  is(
    !!navigator.testTrialGatedAttribute,
    shouldBeActive,
    "Should match active status for Navigator.testTrialControlledAttribute"
  );
  is(
    !!self.TestTrialInterface,
    shouldBeActive,
    "Should match active status for TestTrialInterface"
  );
  if (shouldBeActive) {
    ok(new self.TestTrialInterface(), "Should be able to construct interface");
  }
  // FIXME(emilio): Add more tests.
  //  * Stuff hanging off Window or Document (bug 1757935).
}
