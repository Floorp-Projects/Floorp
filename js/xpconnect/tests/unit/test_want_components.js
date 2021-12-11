function run_test() {
  var sb = Cu.Sandbox(this, {wantComponents: false});

  var rv = Cu.evalInSandbox("this.Components", sb);
  Assert.equal(rv, undefined);
}  
