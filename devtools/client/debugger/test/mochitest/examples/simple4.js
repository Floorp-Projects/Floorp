function funcA() {
  debugger;
  funcB();
  funcC();
}

function funcB() {
  console.log("Hello!");
}

function funcC() {
  console.log("You made it!");
}

funcA();
