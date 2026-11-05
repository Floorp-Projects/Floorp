// Bundle the exact new colocated test body and existing assertion harness.
// Exporting the one test for this focused packaged-product runner does not
// change its statements or assertions. This is not the standard colocated runner.
const fs=require('fs');
const esbuild=require('/workspace/Floorp/node_modules/.deno/esbuild@0.27.7/node_modules/esbuild/lib/main.js');
const root='/workspace/qa-linux-pwa-class-fix';
const path=root+'/browser-features/modules/modules/pwa/test/SsbCommandLineHandler.test.mts';
const source=fs.readFileSync(path,'utf8').replace('async function testLinuxWindowsKeepDistinctLauncherClasses()',
 'export async function testLinuxWindowsKeepDistinctLauncherClasses()');
esbuild.buildSync({stdin:{contents:source,resolveDir:require('path').dirname(path),loader:'ts',sourcefile:path},
 bundle:true,format:'iife',globalName:'qaPwaTests',target:'firefox159',
 outfile:'/workspace/qa-linux-22/package-pwa/pwa-colocated-regression.js'});
