"use strict";

window.test = function () {
  console.log("simple function");
};

window.test_bound_target = function () {
  console.log("simple bound target function");
};

window.test_bound = window.test_bound_target.bind(window);
