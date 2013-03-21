// load() should resolve paths relative to the current script. That's easily
// tested. snarf() aka read() should resolve paths relative to the current
// working directory, which is harder to test because the shell doesn't really
// have any (portable) notion of the current directory (and it can't create
// files to enforce an expected layout.)

loaded = {}
snarfed = {}
relatived = {}
for (let f of ['local.js', '../basic/local.js', 'Y.js']) {
  try {
    load(f);
    loaded[f] = true;
  } catch(e) {
    loaded[f] = !/can't open/.test(e);
  }

  try {
    snarf(f);
    snarfed[f] = true;
  } catch(e) {
    snarfed[f] = !/can't open/.test(e);
  }

  try {
    readRelativeToScript(f);
    relatived[f] = true;
  } catch(e) {
    relatived[f] = !/can't open/.test(e);
  }
}

// local.js in the same dir as this script, so should be found by load() but
// not snarf() -- unless you happen to be in that directory
assertEq(loaded['local.js'], true);
assertEq(loaded['../basic/local.js'], true);
assertEq(relatived['local.js'], true);
assertEq(relatived['../basic/local.js'], true);
if (('PWD' in environment) && !(/test.*[\/\\]basic[\/\\]/.test(environment['PWD']))) {
  assertEq(snarfed['local.js'], false);
  assertEq(snarfed['../basic/local.js'], false);
}

// Y.js is in the root of the objdir, where |make check| is normally
// run from.
assertEq(loaded['Y.js'], false);
assertEq(relatived['Y.js'], false);
if (!snarfed['Y.js']) {
  print("WARNING: expected to be able to find Y.js in current directory\n");
  print("(not failing because it depends on where this test was run from)\n");
}
