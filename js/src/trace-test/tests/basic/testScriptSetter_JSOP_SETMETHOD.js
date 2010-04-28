function s(f) { this._m = f; }

function C(i) {
    Object.defineProperty(this, "m", {set: s});
    this.m = function () { return 17; };
}

var arr = [];
for (var i = 0; i < 9; i++)
    arr[i] = new C(i);

checkStats({recorderStarted: 1, recorderAborted: 0, traceCompleted: 1, traceTriggered: 1});

//BUG - uncomment this when bug 559912 is fixed
//for (var i = 1; i < 9; i++)
//    assertEq(arr[0]._m === arr[i]._m, false);
