// |jit-test| error: TypeError
var obj = {};
let ([] = print) 3; 
let ( i = "a" ) new i [ obj[i] ];
