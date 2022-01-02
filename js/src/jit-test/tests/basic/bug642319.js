
test();
function test() {
        function f() {
                function test( ) { summary( summary, test, false ); }
        }
        f.__proto__ = this;
}
gc();
test();
