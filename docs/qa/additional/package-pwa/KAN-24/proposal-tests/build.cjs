const esbuild=require('/workspace/Floorp/node_modules/.deno/esbuild@0.27.7/node_modules/esbuild/lib/main.js');
const fs=require('fs'),path=require('path'),crypto=require('crypto');
const root='/workspace/qa-kan24-pr2950',out=__dirname;
const observer=root+'/browser-features/chrome/common/designs/utils/vertical-tabs-observer.ts';
const manager=root+'/browser-features/pages-settings/src/app/design/dataManager.ts';
const entry=`export {addVerticalTabsObserver} from '${observer}';export {getDesignSettings,saveDesignSettings} from '${manager}';export {rpc} from '${root}/browser-features/pages-settings/src/lib/rpc/rpc.ts';export {usesSettingsActor} from '${root}/libs/ui/settings-rpc-origin.ts';`;
const meta={head:'69d306b11c9e52da9e6270870740d49b1d561659',sources:{},variants:{}};
(async()=>{for(const variant of ['canonical','proposed']){
 const patches={};
 if(variant==='proposed'){
  patches[observer]=fs.readFileSync(observer,'utf8').replace('Services.prefs.addObserver(VERTICAL_TABS_PREF, onPrefChange);','Services.prefs.addObserver(VERTICAL_TABS_PREF, onPrefChange);\n  onPrefChange();');
  patches[manager]=fs.readFileSync(manager,'utf8').replace(`  const tabbarStyle =\n    !hasTabStyleChanged && nativeVerticalTabs !== null\n      ? resolveTabbarStyleFromNative(nativeVerticalTabs, requestedStyle)\n      : requestedStyle;`,`  const tabbarStyle = hasTabStyleChanged\n    ? requestedStyle\n    : nativeVerticalTabs !== null\n    ? resolveTabbarStyleFromNative(nativeVerticalTabs, storedStyle)\n    : storedStyle;`);
 }
 await esbuild.build({stdin:{contents:entry,loader:'ts',resolveDir:root},bundle:true,format:'iife',globalName:'qaKan24',target:'firefox159',define:{'import.meta.env.DEV':'false'},nodePaths:['/workspace/Floorp/node_modules'],alias:{'@':root+'/browser-features/pages-settings/src','#features-chrome':root+'/browser-features/chrome','#libs':root+'/libs'},plugins:[{name:'proposal-only',setup(b){b.onLoad({filter:/\.css(?:\?inline)?$/},args=>({contents:fs.readFileSync(args.path.split('?')[0],'utf8'),loader:'text'}));b.onLoad({filter:/\.ts$/},args=>{if(patches[args.path])return {contents:patches[args.path],loader:'ts'};});}}],outfile:path.join(out,variant+'.js')});
 for(const file of [observer,manager])meta.sources[path.relative(root,file)]=crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
 meta.variants[variant]={bundleSha256:crypto.createHash('sha256').update(fs.readFileSync(path.join(out,variant+'.js'))).digest('hex'),transformedFiles:Object.keys(patches).map(f=>path.relative(root,f))};
 for(const [file,text]of Object.entries(patches))fs.writeFileSync(path.join(out,'proposed-'+path.basename(file)),text);
 }fs.writeFileSync(path.join(out,'build-provenance.json'),JSON.stringify(meta,null,2));})();
