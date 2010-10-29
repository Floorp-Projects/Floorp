function testIn() {
    var array = [3];
    var obj = { "-1": 5, "1.7": 3, "foo": 5, "1": 7 };
    var a = [];
    for (let j = 0; j < 5; ++j) {
        a.push("0" in array);
        a.push(-1 in obj);
        a.push(1.7 in obj);
        a.push("foo" in obj);
        a.push(1 in obj);
        a.push("1" in array);
        a.push(-2 in obj);
        a.push(2.7 in obj);
        a.push("bar" in obj);
        a.push(2 in obj);
    }
    return a.join(",");
}
assertEq(testIn(), "true,true,true,true,true,false,false,false,false,false,true,true,true,true,true,false,false,false,false,false,true,true,true,true,true,false,false,false,false,false,true,true,true,true,true,false,false,false,false,false,true,true,true,true,true,false,false,false,false,false");
