import { fancySort } from "./sorted.js";

window.test = function originalTestName() {
  let test = ["b (30)", "a", "b (5)", "z"];
  let result = fancySort(test);
  console.log(result);
};
