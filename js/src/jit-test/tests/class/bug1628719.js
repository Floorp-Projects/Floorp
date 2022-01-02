class BaseOne {
  static build() { return 'BaseOne'; }
}

class BaseTwo {
  static build() { return 'BaseTwo'; }
}

class ClassOne extends BaseOne {
  constructor() { super(); }
}

class ClassTwo extends BaseTwo {
  constructor() { super(); }
}

const ClassMap = { 1: ClassOne, 2: ClassTwo };
const TimeLine = [2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2];

function run() {
  for (var i = 0; i < TimeLine.length; ++i) {
    var j = TimeLine[i];
    var expected = j === 1 ? 'BaseOne' : 'BaseTwo';
    var actual = ClassMap[j].build();
    assertEq(actual, expected);
  }
}
run();
