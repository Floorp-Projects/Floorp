var g = newGlobal();
g.eval("this.inner = getSelfHostedValue('outer')()");
gc();
g.inner();
