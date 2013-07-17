for (var i = -10; i < 10; i++)
    assertNear(Math.log2(Math.pow(2, i)), i);

assertNear(Math.log2(5), 2.321928094887362);
assertNear(Math.log2(7), 2.807354922057604);
assertNear(Math.log2(Math.E), Math.LOG2E);

reportCompare(0, 0, "ok");
