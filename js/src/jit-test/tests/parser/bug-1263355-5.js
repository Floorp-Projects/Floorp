// |jit-test| error: ReferenceError

new class extends Object { constructor(a = (()=>{delete super[super()]})()) { } }
