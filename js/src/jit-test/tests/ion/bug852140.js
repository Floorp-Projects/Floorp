function reportCompare (expected, actual) {
  if (expected != actual) {}
}
function exitFunc (funcName) {
  return reportCompare(undefined, '');
}
reportCompare('', '');
exitFunc();
