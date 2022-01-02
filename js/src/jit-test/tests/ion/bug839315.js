function f(x) {
    switch(x) {
      case 0:
      case 100:
    }
}
f("");
evaluate('f({})', { noScriptRval : true });

function g(x) {
    switch(x) {
      case 0.1:
      case 100:
    }
}

g(false);
evaluate('g({})', { noScriptRval : true });
