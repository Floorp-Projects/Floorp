load(libdir + 'eqArrayHelper.js');
assertEqArray(/((a|)+b)+/.exec('bb'), [ "bb", "b", "" ]);
