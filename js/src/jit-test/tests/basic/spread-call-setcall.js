load(libdir + "asserts.js");

function g() {
}

let a = {
  g: function() {
  }
};

function check(expr) {
  assertThrowsInstanceOf(Function(expr), ReferenceError);
}

check("g(...[]) = 1");
check("a.g(...[]) = 1");
check("eval(...['1']) = 1");
check("[g(...[])] = 1");
check("[a.g(...[])] = 1");
check("[eval(...['1'])] = 1");
check("({y: g(...[])}) = 1");
check("({y: a.g(...[])}) = 1");
check("({y: eval(...['1'])}) = 1");
check("g(...[]) ++");
check("a.g(...[]) ++");
check("eval(...['1']) ++");
