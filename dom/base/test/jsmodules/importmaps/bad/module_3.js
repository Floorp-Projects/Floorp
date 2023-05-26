import {} from "../circular_depdendency.js";

export function exportedFunction() {
  throw "Wrong version of function called";
}

throw "Shouldn't laod file bad/module_3.js";
