// |jit-test| error: TypeError

(new class extends Array {constructor(a=()=>eval("super()")){ var f = ()=>super(); f() }})(0)
