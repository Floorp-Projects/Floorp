h = Function("a", "b", "c", "...rest", "return rest.toString();");
assertEq(h.length, 3);
assertEq(h(1, 2, 3, 4, 5), "4,5");
