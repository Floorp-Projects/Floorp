function f() {}
for (var i = 0; i < HOTLOOP; ++i) {
    f.call(null);
}

checkStats({
    recorderStarted: 1,
    recorderAborted: 0
});
