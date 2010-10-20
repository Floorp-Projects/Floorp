function testNestedEscapingLambdas()
{
    try {
        return (function() {
            var a = [], r = [];
            function setTimeout(f, t) {
                a.push(f);
            }

            function runTimeouts() {
                for (var i = 0; i < a.length; i++)
                    a[i]();
            }

            var $foo = "#nothiddendiv";
            setTimeout(function(){
                r.push($foo);
                setTimeout(function(){
                    r.push($foo);
                }, 100);
            }, 100);

            runTimeouts();

            return r.join("");
        })();
    } catch (e) {
        return e;
    }
}
assertEq(testNestedEscapingLambdas(), "#nothiddendiv#nothiddendiv");
