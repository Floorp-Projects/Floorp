const esbuild = require('/workspace/Floorp/node_modules/.deno/esbuild@0.27.7/node_modules/esbuild/lib/main.js');
const fs = require('fs'), path = require('path'), cp = require('child_process'), crypto = require('crypto');
const root='/workspace/qa-kan26-lepton-fix', out=__dirname;
const subject='browser-features/chrome/common/designs/utils/lepton-compat-152.css.ts';
const entry=root+'/browser-features/chrome/common/designs/test/lepton-container-geometry.test.ts';
(async()=>{const metadata={testSource:entry,variants:{}};for(const variant of ['baseline','candidate']){
 await esbuild.build({entryPoints:[entry],bundle:true,format:'iife',globalName:'qaLeptonGeometry',target:'firefox159',alias:{'@nora/skin':root+'/browser-features/skin'},plugins:[{name:'actual-source',setup(b){b.onResolve({filter:/\.css\?raw$/},args=>({path:path.resolve(root+'/browser-features/skin',args.path.replace('@nora/skin/','').replace('?raw','')),namespace:'raw'}));b.onLoad({filter:/.*/,namespace:'raw'},a=>({contents:fs.readFileSync(a.path,'utf8'),loader:'text'}));b.onLoad({filter:/lepton-compat-152\.css\.ts$/},a=>({contents:variant==='baseline'?cp.execFileSync('git',['-C',root,'show','29a3eb23145da90706d8a94441373278521cc737:'+subject],{encoding:'utf8'}):fs.readFileSync(a.path,'utf8'),loader:'ts'}));}}],outfile:path.join(out,variant+'-test.js')});metadata.variants[variant]={bundleSha256:crypto.createHash('sha256').update(fs.readFileSync(path.join(out,variant+'-test.js'))).digest('hex')};}
fs.writeFileSync(path.join(out,'test-build.json'),JSON.stringify(metadata,null,2)+'\n');})();
