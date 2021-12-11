var interestingCaptureNums = [(1 << 14),
                              (1 << 15) - 1,
                              (1 << 15),
                              (1 << 15) + 1,
                              (1 << 16)]

for (let i of interestingCaptureNums) {
    print(i);
    try {
        var source = Array(i).join("(") + "a" + Array(i).join(")");
        RegExp(source).exec("a");
    } catch {}
}
