class Base {}
class Derived extends Base {
    constructor() {
        var fun = async() => {
            for (var i = 0; i < 20; i++) {} // Trigger OSR.
            super();
        };
        fun();
    }
}
d = new Derived();
