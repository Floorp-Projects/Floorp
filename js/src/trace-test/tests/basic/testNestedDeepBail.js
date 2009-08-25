var _quit;
function testNestedDeepBail()
{
    _quit = false;
    function loop() {
        for (var i = 0; i < 4; i++)
            ;
    }
    loop();

    function f() {
        loop();
        _quit = true;
    }
    var stk = [[1], [], [], [], []];
    while (!_quit)
        stk.pop().forEach(f);
}
testNestedDeepBail();
