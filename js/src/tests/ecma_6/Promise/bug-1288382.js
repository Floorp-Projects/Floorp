if (!this.Promise) {
    this.reportCompare && reportCompare(true, true);
    quit(0);
}

// This just shouldn't trigger a failed assert.
// It does without bug 1288382 fixed.
Promise.all.call(class {
  constructor(exec){ exec(()=>{}, ()=>{}); }
  static resolve() { return {then(){}}; }
}, [null]);

this.reportCompare && reportCompare(true, true);
