
var total = 0;

print("all_bench");
print("\n");

load("getProp_bench.js");
print("\n");

load("setProp_bench.js");
print("\n");

load("add_bench.js");
print("\n");

load("inc_bench.js");
print("\n");

load("math_bench.js");
print("\n");

load("func_bench.js");
print("\n");

load("branch_bench.js");
print("\n");

load("new_bench.js");
print("\n");

load("array_bench.js");
print("\n");

load("misc_bench.js");
print("\n");

load("eval_bench.js");
print("\n");

print("Total execution time was : " + total + " ms.");