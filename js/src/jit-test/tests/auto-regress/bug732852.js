// |jit-test| error:SyntaxError

// Binary: cache/js-dbg-64-1fd6c40d3852-linux
// Flags: --ion-eager
//

var ary = ["\u001Cfoo", "\u001Dfoo", "\u001Efoo", "\u001Ffoo"];
for (var i in ary) {
  ary[Number(i)].search("var MYVAR='077';++MYVAR")
}
