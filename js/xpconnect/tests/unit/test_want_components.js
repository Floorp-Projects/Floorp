function run_test() {
  var sb;

  sb = Cu.Sandbox(this, {wantComponents: false});
  Assert.equal(Cu.evalInSandbox("this.Components", sb), undefined);
  Assert.equal(Cu.evalInSandbox("this.Services", sb), undefined);

  sb = Cu.Sandbox(this, {wantComponents: true});
  Assert.equal(Cu.evalInSandbox("typeof this.Components", sb), "object");
  Assert.equal(Cu.evalInSandbox("typeof this.Services", sb), "object");

  // wantComponents defaults to true.
  sb = Cu.Sandbox(this, {});
  Assert.equal(Cu.evalInSandbox("typeof this.Components", sb), "object");
  Assert.equal(Cu.evalInSandbox("typeof this.Services", sb), "object");
}
