function f() {
  try {} catch (e) {}
}
function g(code) {
  function m() {
    return "(function(){return " + code + "})()"
  }
  var codeNestedDeep = m(codeNestedDeep)
  h(m(code), "same-compartment")
  h(codeNestedDeep, "same-compartment")
}
function h(code, globalType) {
  try {
    evalcx(code, newGlobal(globalType))
  } catch (e) {
    "" + f()
  }
}
function p()(function() function() {})
g("print(let(x=verifyprebarriers(),q)((x(\"\",l('')))?(\"\"):(\"\")))()")
