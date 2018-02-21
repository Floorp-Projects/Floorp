function bar() {
  return new Promise(resolve => setTimeout(resolve, 100))
}




async function foo() {
  await bar();
  console.log("YO")
}

console.log("HEY")
