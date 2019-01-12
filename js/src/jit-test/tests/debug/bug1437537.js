// Don't assert when pausing for onStep at JSOP_EXCEPTION.

var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);
let f = g.Function(`try { throw new Error; } catch (e) { return 'natural'; }`);

let limit = -1;
dbg.onEnterFrame = function (frame) {
    frame.onStep = function () {
        if (this.offset > limit) {
            limit = this.offset;
            return { return: 'forced' };
        }
    };
};

while (f() === 'forced') {
}
