console.log("only one breakable line");
// And one non-breakable line

function nonSourceMappedFunction () {
  console.log("non source mapped function");
  // This will call a function from removed-original.js
  return removedOriginal();
}
