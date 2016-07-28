var assert = {
    sameValue: assertEq,
    notSameValue(a, b, msg) {
      try {
        assertEq(a, b);
        throw "equal"
      } catch (e) {
        if (e === "equal")
          throw new Error("Assertion failed: expected different values, got " + a);
      }
    },
    throws(ctor, f) {
      var fullmsg;
      try {
        f();
      } catch (exc) {
        if (exc instanceof ctor)
          return;
        fullmsg = "Assertion failed: expected exception " + ctor.name + ", got " + exc;
      }
      if (fullmsg === undefined)
        fullmsg = "Assertion failed: expected exception " + ctor.name + ", no exception thrown";
      if (msg !== undefined)
        fullmsg += " - " + msg;
      throw new Error(fullmsg);
    }
}
