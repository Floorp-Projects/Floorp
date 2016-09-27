args = ""
for (i = 0; i < 2000; i++) {
    args += "arg" + i;
    if (i != 1999) args += ",";
}
MyFunc = MyObject = Function(args, "for (var i = 0; i < MyFunc.length; i++ )   break; eval('this.arg'+i +'=arg'+i) ");
new function TestCase() {
    if (inIon())
        return;
    TestCase(eval("var EXP_1 = new MyObject; var EXP_2 = new MyObject; EXP_1 - EXP_2"));
}
