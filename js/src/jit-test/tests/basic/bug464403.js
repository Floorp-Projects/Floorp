function bug464403() {
    print(8);
    var u = [print, print, function(){}]
    for each (x in u) for (u.e in [1,1,1,1]);
    return "ok";
}
assertEq(bug464403(), "ok");
