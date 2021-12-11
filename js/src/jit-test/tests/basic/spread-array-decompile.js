var samples = [
    "[...a]",
    "[...[1]]",
    "[1, ...a, 2]",
    "[1, ...[2, 3], 4]",
    "[...[1], , ]",
    "[1, , ...[2]]",
    "[, 1, ...[2], ...[3], , 4, 5, , ]"
];
for (var sample of samples) {
    var source = "function f() {\n    return " + sample + ";\n}";
    eval(source);
    assertEq(f.toString(), source);
}
