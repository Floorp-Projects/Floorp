window.bar = function bar() {
  return new Promise(resolve => setTimeout(resolve, 100))
}

window.foo = async function foo() {
  await bar();
  console.log("YO")
}
