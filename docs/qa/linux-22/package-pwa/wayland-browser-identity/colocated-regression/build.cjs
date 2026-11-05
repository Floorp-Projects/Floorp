// Bundle the unmodified colocated module and its repository assertion runner.
const esbuild = require('/workspace/Floorp/node_modules/.deno/esbuild@0.27.7/node_modules/esbuild/lib/main.js');
esbuild.buildSync({
  entryPoints: ['/workspace/qa-linux-wayland-browser-fix/browser-features/modules/modules/pwa/test/SsbCommandLineHandler.test.mts'],
  bundle: true,
  format: 'iife',
  globalName: 'qaPwaTests',
  target: 'firefox159',
  outfile: __dirname + '/exact-tests.js',
});
