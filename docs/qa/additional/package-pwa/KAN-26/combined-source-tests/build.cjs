const esbuild = require('/workspace/Floorp/node_modules/.deno/esbuild@0.27.7/node_modules/esbuild/lib/main.js');
const fs=require('fs'),path=require('path'),cp=require('child_process'),crypto=require('crypto');
const root='/workspace/qa-kan26-nonphoton',out=__dirname;
const paths=[
'browser-features/chrome/common/designs/test/lepton-container-geometry.test.ts',
'browser-features/chrome/common/designs/test/fluerial-container.test.ts',
'browser-features/chrome/common/designs/utils/lepton-compat-152.css.ts',
'browser-features/chrome/common/designs/utils/tab-color-like-toolbar.css.ts',
'browser-features/chrome/common/designs/utils/gecko-152-var-aliases.css.ts',
'browser-features/skin/lepton/css/leptonChrome.css',
'browser-features/skin/fluerial/css/fluerial.css',
'browser-features/chrome/test/utils/test_harness.ts'];
const head=cp.execFileSync('git',['-C',root,'rev-parse','HEAD'],{encoding:'utf8'}).trim();
if(head!=='3a9f99ae12dbb221f63b28ec210b04fafcf2be15')throw new Error('Wrong source head');
const entry=`import{runAllTests as lepton}from'${root+'/'+paths[0]}';import{runAllTests as fluerial}from'${root+'/'+paths[1]}';export async function runAllTests(){const modules=[];for(const[name,run]of[['lepton-container-geometry',lepton],['fluerial-container.test.ts',fluerial]]){const start=Date.now();try{await run();modules.push({module:name,pass:true,elapsedMs:Date.now()-start,progress:{...globalThis.__NORA_TEST_PROGRESS__}});}catch(error){modules.push({module:name,pass:false,error:String(error),stack:error.stack,elapsedMs:Date.now()-start,progress:{...globalThis.__NORA_TEST_PROGRESS__}});}}return{modules,allPass:modules.every(x=>x.pass),declaredCaseCount:4};}`;
(async()=>{await esbuild.build({stdin:{contents:entry,loader:'ts',resolveDir:root},bundle:true,format:'iife',globalName:'qaCombined',target:'firefox159',alias:{'@nora/skin':root+'/browser-features/skin'},plugins:[{name:'exact-vendor-css',setup(b){b.onResolve({filter:/\.css\?raw$/},a=>({path:path.resolve(root+'/browser-features/skin',a.path.replace('@nora/skin/','').replace('?raw','')),namespace:'raw'}));b.onLoad({filter:/.*/,namespace:'raw'},a=>({contents:fs.readFileSync(a.path,'utf8'),loader:'text'}));}}],outfile:out+'/candidate-test.js'});const status=cp.execFileSync('git',['-C',root,'status','--short'],{encoding:'utf8'}).trim();const sources=paths.map(p=>({path:p,gitBlob:cp.execFileSync('git',['-C',root,'rev-parse','HEAD:'+p],{encoding:'utf8'}).trim(),sha256:crypto.createHash('sha256').update(fs.readFileSync(root+'/'+p)).digest('hex')}));const metadata={sourceSha:head,sourceTrackedChanges:status,kind:'exact combined source browser regression modules; not full candidate package or standard top-level runner qualification',sources,bundleSha256:crypto.createHash('sha256').update(fs.readFileSync(out+'/candidate-test.js')).digest('hex'),modules:[{name:'lepton-container-geometry',declaredCases:2},{name:'fluerial-container.test.ts',declaredCases:2}]};fs.writeFileSync(out+'/build-provenance.json',JSON.stringify(metadata,null,2)+'\n');console.log(JSON.stringify({head,sourceTrackedChanges:status,bundleSha256:metadata.bundleSha256}));})();
