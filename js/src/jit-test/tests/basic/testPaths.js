// load() and snarf() (aka read()) should resolve paths relative to the current
// working directory. This is a little hard to test because the shell doesn't
// really have any (portable) notion of the current directory (and it can't
// create files to enforce an expected layout.) loadRelativeToScript() and
// readRelativeToScript() do what their names say, which is much easier to
// test.

loaded = {}
snarfed = {}
loadRel = {}
snarfRel = {}
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
    snarfRel[f] = true;
  } catch(e) {
    snarfRel[f] = !/can't open/.test(e);
  }

  try {
    loadRelativeToScript(f);
    loadRel[f] = true;
  } catch(e) {
    loadRel[f] = !/can't open/.test(e);
  }
}

// local.js in the same dir as this script, so should be found by the
// script-relative calls but not the cwd-relative ones -- unless you happen to
// be in that directory
assertEq(loadRel['local.js'], true);
assertEq(loadRel['../basic/local.js'], true);
assertEq(snarfRel['local.js'], true);
assertEq(snarfRel['../basic/local.js'], true);
if (('PWD' in environment) && !(/test.*[\/\\]basic[\/\\]/.test(environment['PWD']))) {
  assertEq(loaded['local.js'], false);
  assertEq(loaded['../basic/local.js'], false);
  assertEq(snarfed['local.js'], false);
  assertEq(snarfed['../basic/local.js'], false);
}

// Y.js is in the root of the objdir, where |make check| is normally
// run from.
assertEq(loadRel['Y.js'], false);
assertEq(snarfRel['Y.js'], false);
if (!snarfed['Y.js']) {
  print("WARNING: expected to be able to find Y.js in current directory\n");
  print("(not failing because it depends on where this test was run from)\n");
}
if (!loaded['Y.js']) {
  print("WARNING: expected to be able to find Y.js in current directory\n");
  print("(not failing because it depends on where this test was run from)\n");
}
