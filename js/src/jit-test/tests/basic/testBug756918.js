// |jit-test| error:Error

with({})
  let([] = []) {
    eval("throw new Error()");
  }
