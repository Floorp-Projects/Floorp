function fn() {
  var nonlex;
  let lex;

}

function fn2() {
  function nonlex(){}
  let lex;

}

function fn3() {
  var nonlex;
  class Thing {}

}

function fn4() {
  function nonlex(){}
  class Thing {}

}
