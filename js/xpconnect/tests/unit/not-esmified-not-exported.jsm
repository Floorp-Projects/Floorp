var exportedVar = "exported var";
function exportedFunction() {
  return "exported function";
}
let exportedLet = "exported let";
const exportedConst = "exported const";

var notExportedVar = "not exported var";
function notExportedFunction() {
  return "not exported function";
}
let notExportedLet = "not exported let";
const notExportedConst = "not exported const";

const EXPORTED_SYMBOLS = [
  "exportedVar",
  "exportedFunction",
  "exportedLet",
  "exportedConst",
];
