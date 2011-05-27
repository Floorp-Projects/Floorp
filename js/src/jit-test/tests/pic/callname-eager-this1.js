this.name = "outer";

var sb = evalcx('');
sb.name = "inner";
sb.parent = this;

function f() {
    assertEq(this.name, "inner");
}

evalcx('with(this) { ff = parent.f; }; (function() { eval(""); for(var i=0; i<10; i++) { ff() } })()', sb);
