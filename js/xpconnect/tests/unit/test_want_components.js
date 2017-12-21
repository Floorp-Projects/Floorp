function run_test() {
  var cu = Components.utils;
  var sb = cu.Sandbox(this, 
	    {wantComponents: false});

  var rv = cu.evalInSandbox("this.Components", sb);
  Assert.equal(rv, undefined);
}  
