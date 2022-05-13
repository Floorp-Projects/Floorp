window.bar = function bar() {
  return new Promise(resolve => setTimeout(resolve, 100))
}

// New comment
// to add some
// non-breakable lines
window.foo = async function foo() {
  await bar();
  console.log("YO")
}

console.log("HEY")
