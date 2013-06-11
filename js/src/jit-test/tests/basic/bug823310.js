"use strict";
options("werror");

// This construct causes a strict warning, but we shouldn't get one since
// JSOPTION_STRICT isn't enabled.
var x;
eval("if (x = 3) {}");
