load('tests/cli.js');

JSAN.use('MochiKit.Test');

print("[[ MochiKit.Base ]]");
runTests('tests.test_Base');
print("[[ MochiKit.Color ]]");
runTests('tests.test_Color');
print("[[ MochiKit.DateTime ]]");
runTests('tests.test_DateTime');
print("[[ MochiKit.Format ]]");
runTests('tests.test_Format');
print("[[ MochiKit.Iter ]]");
runTests('tests.test_Iter');
print("[[ MochiKit.Logging ]]");
runTests('tests.test_Logging');
