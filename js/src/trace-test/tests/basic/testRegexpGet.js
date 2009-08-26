function testRegexpGet() {
    var re = /hi/;
    var a = [];
    for (let i = 0; i < 5; ++i)
        a.push(re.source);
    return a.toString();
}
assertEq(testRegexpGet(), "hi,hi,hi,hi,hi");
