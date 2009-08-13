function testClosures()
{
    function MyObject(id) {
        var thisObject = this;
        this.id = id;
        this.toString = str;

        function str() {
            return "" + this.id + thisObject.id;
        }
    }

    var a = [];
    for (var i = 0; i < 5; i++)
        a.push(new MyObject(i));
    return a.toString();
}
assertEq(testClosures(), "00,11,22,33,44");
