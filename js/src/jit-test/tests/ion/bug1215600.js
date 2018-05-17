|jit-test| slow

lfcode = Array()
lfcode.push("5")
lfcode.push("")
lfcode.push("3")
lfcode.push("oomTest(()=>{gc()})")
for (let i = 0; i < 10; i++) {
    file = lfcode.shift()
    loadFile(file)
}
function loadFile(lfVarx) {
    try {
        if (lfVarx.length != 1)
            switch (lfRunTypeId) {
                case 3:
                    function newFunc(x) { return Function(x)(); }
                    newFunc(lfVarx)
                case 5:
                    for (lfLocal in this);
            }
        isNaN();
        lfRunTypeId = parseInt(lfVarx);
    } catch (lfVare) {}
}
