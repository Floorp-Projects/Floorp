enableGeckoProfiling();

class base {}
class derived extends base {
    testElem() {
        super[ruin()];
    }
}
let instance = new derived();
try {
    instance.testElem();
} catch { /* don't crash */ }
