// |jit-test| error: TypeError 
// don't assert

print(this.watch("x",
function() {
  Object.defineProperty(this, "x", ({
    get: (Int8Array)
  }))
}))(x = /x/)
