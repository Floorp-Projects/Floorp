window.bar = function bar() {
  return new Promise(resolve => setTimeout(resolve, 100))
}

window.foo = async function foo() {
  // This will call a function from script.js, itself calling a function
  // from original-with-query.js
  await nonSourceMappedFunction();
  console.log("YO")
}
