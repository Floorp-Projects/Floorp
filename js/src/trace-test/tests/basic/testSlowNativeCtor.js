delete _quit;

function testSlowNativeCtor() {
    for (var i = 0; i < 4; i++)
	new Date().valueOf();
}
testSlowNativeCtor();
