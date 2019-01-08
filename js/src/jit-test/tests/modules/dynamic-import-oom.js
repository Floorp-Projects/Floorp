// |jit-test| skip-if: !('oomTest' in this); --ion-offthread-compile=off
//
// Note: without --ion-offthread-compile=off this test takes a long time and
// may timeout on some platforms. See bug 1507721.

ignoreUnhandledRejections();

oomTest(() => import("module1.js"));
oomTest(() => import("cyclicImport1.js"));
