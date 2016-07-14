if (typeof verifyprebarriers != 'function' ||
    typeof offThreadCompileScript != 'function')
    quit();

try {
    // This will fail with --no-threads.
    verifyprebarriers();
    var lfGlobal = newGlobal();
    lfGlobal.offThreadCompileScript(`
      version(185);
    `);
}
catch (e) {
    quit(0);
}

