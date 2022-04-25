// This content of this file is added before that of original-with-no-update.js in the generated file
// bundle-with-another-original.js, its main goal is to just cause the content of the generated file to change.
function funcA() {
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
