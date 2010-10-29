var o = {
  valueOf : function() {
    return {
      toString : function() { return "fail" }
    }
  },
  toString : function() { return "good" }
};

var p = {
  valueOf : function() {
    return {
      toString : function() { return "0" }
    }
  },
  toString : function() { return "7" }
};

var q = {
  toString : function() {
    return {
      valueOf : function() { return "0" }
    }
  },
  valueOf : function() { return "7" }
};

function assert(b, s) {
    if (b)
        return;
    assertEq("imacro produces incorrect result for " + s, "fail");
}

function run() {
    for (var i = 0; i < 5; ++i) {
        // equality / inequality
        assert(!(o == "fail"), "obj == any");
        assert(!("fail" == o), "any == obj");
        assert(!(o != "good"), "obj != any");
        assert(!("good" != o), "any != obj");

        // binary
        assert(!((p | 3) != 7), "obj | any");
        assert(!((3 | p) != 7), "any | obj");
        assert(!((p | p) != 7), "obj | obj");
        assert(!((p & 3) != 3), "obj & any");
        assert(!((3 & p) != 3), "any & obj");
        assert(!((p & p) != 7), "obj & obj");
        assert(!((p * 3) != 21), "obj * any");
        assert(!((3 * p) != 21), "any * obj");
        assert(!((p * p) != 49), "obj * obj");

        // addition
        assert(!(o + "" != "good"), "obj + any");
        assert(!("" + o != "good"), "any + obj");
        assert(!(o + o != "goodgood"), "any + any");

        // sign
        assert(!(-p != -7), "-obj");
        assert(!(+p != 7), "+obj");

        // String
        assert(!(String(q) != "7"), "String(obj)");
        assert(!(new String(q) != "7"), "new String(obj)");
    }
    return true;
}

run();
