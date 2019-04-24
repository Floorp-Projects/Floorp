// |jit-test| error:Error

let sandbox = evalcx("lazy");

let domObject = new FakeDOMObject();
let {object, transplant} = transplantableObject({object: domObject});

transplant(sandbox);
