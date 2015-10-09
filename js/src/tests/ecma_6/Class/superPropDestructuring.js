var test = `

class base {
    constructor() { }
}

let seenValues;
Object.defineProperty(base.prototype, "minutes",
                      {
                        set(x) {
                            assertEq(x, 525600);
                            seenValues.push(x);
                        }
                      });
Object.defineProperty(base.prototype, "intendent",
                      {
                        set(x) {
                            assertEq(x, "Fred");
                            seenValues.push(x)
                        }
                      });

const testArr = [525600, "Fred"];
class derived extends base {
    constructor() { super(); }
    prepForTest() { seenValues = []; }
    testAsserts() { assertDeepEq(seenValues, testArr); }
    testProps() {
        this.prepForTest();
        [super.minutes, super.intendent] = testArr;
        this.testAsserts();
    }
    testElems() {
        this.prepForTest();
        [super["minutes"], super["intendent"]] = testArr;
        this.testAsserts();
    }
}

let d = new derived();
d.testProps();
d.testElems();

`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
