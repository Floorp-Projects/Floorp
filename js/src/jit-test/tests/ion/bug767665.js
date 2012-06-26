var failures = 0;
function a() {
	return new Array(-1);
}
for (var j = 0; j < 61; ++j) {
	try {
		a();
		++failures;
	} catch (e) {
	}
}
