function testResumeOp() {
    var a = [1,"2",3,"4",5,"6",7,"8",9,"10",11,"12",13,"14",15,"16"];
    var x = "";
    while (a.length > 0)
        x += a.pop();
    return x;
}
assertEq(testResumeOp(), "16151413121110987654321");
