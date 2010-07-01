var data = {
  answer: 42,
  nested: { objects: { work: "yes they do" } },
  arr: [
    "zeroeth",
    { foo: "bar" },
    function() { return data },
    { toString: function() { return "last" } }
  ],
  toString: function() {
    return "CPOW";
  }
};

var empty = function() {
  this.try_to_delete = "just try";
};
empty.prototype = {
  try_to_delete: "bwahaha",
  inherited: "inherited",
  method: function() {
    return "called"
  }
};
data.derived = new empty;

(data.constructor = function(value) {
  var self = this;
  this.value = value;
  this.check = function(that) {
    do_check_eq(this.value, that.value);
    do_check_eq(this, self);
    do_check_eq(this, that);
    do_check_false(this.isGlobal());
  };
}).prototype = {
  isGlobal: function() {
    return (function() { return this })() == this;
  }
};

function A() {
  this.a = A;
  this.b = A;
}
function B() {
  this.b = B;
  this.c = B;
}
B.prototype = new A;

function pitch(ball) {
  throw ball;
}

get_set = {
  get foo() { return 42; },
  get foo_throws() { throw "BAM"; },
  set one(val) { this.two = val + 1; }
};

function type(x) {
  return typeof x;
}

function run_test() {}
