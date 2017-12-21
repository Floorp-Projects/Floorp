function run_test() {
  // Make sure nsJSID implements classinfo.
  Assert.equal(Components.ID("{a6e2a27f-5521-4b35-8b52-99799a744aee}").equals, Components.ID("{daa47351-7d2e-44a7-b8e3-281802a1eab7}").equals);
}
