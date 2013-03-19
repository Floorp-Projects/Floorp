function reportCompare (expected, actual) {
  if (expected != actual) {}
}
function exitFunc (funcName)
  reportCompare(undefined, '');
reportCompare('', '');
exitFunc();
