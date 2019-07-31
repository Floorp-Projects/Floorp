// One Debugger's onPop handler can disable another Debugger.
var g = newGlobal({newCompartment: true});
var dbg1 = new Debugger(g);
var dbg2 = new Debugger(g);

var log;
var frames = [];
var firstPop = true;

function handleEnter(frame) {
    log += '(';
    frames.push(frame);
    frame.debugger = this;
    frame.onPop = function handlePop(completion) {
        log += ')';
        assertEq(completion.return, 42);
        if (firstPop) {
            // We can't say which frame's onPop handler will get called first.
            if (this == frames[0])
                frames[1].debugger.enabled = false;
            else
                frames[0].debugger.enabled = false;
        } else {
            assertEq("second pop handler was called",
                     "second pop handler should not be called");
        }
        firstPop = false;
    };
};

dbg1.onEnterFrame = handleEnter;
dbg2.onEnterFrame = handleEnter;

log = '';
assertEq(g.eval('40 + 2'), 42);
assertEq(log, '(()');
