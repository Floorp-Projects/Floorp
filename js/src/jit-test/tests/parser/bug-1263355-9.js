// |jit-test| error: ReferenceError

let a;
for(let {a = new class extends Array { constructor(b = (a = eval("()=>super()"))){} }} of [[]]) {
}
