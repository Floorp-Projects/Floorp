#!/usr/bin/env python3
"""Bounded standard collector capability probe, not a product regression run."""
from pathlib import Path
import os,sys,time,subprocess,json,hashlib,zipfile,traceback
root=Path(__file__).resolve().parent/sys.argv[1];root.mkdir(exist_ok=False);repo=Path('/workspace/qa-linux-full-build/floorp3030');product=Path('/workspace/qa-linux-full-build/products/3030/floorp');base=Path('/workspace/qa-linux-22/package-pwa');sys.path.insert(0,str(base));from pwa_ui_probe import Marionette
home=root/'home';profile=root/'profile';runtime=root/'runtime';[x.mkdir(exist_ok=False) for x in [home,profile,runtime]];runtime.chmod(0o700)
(profile/'user.js').write_text('user_pref("marionette.port",2923);\n')
def inventory():return {str(f.relative_to(product)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(product.rglob('*')) if f.is_file()}
def save(n,d):(root/(n+'.json')).write_text(json.dumps(d,indent=2)+'\n')
before=inventory();save('product-before',before)
env=dict(os.environ,HOME=str(home),XDG_CONFIG_HOME=str(home/'.config'),XDG_DATA_HOME=str(home/'.local/share'),XDG_CACHE_HOME=str(home/'.cache'),XDG_RUNTIME_DIR=str(runtime),DISPLAY=':105',GDK_BACKEND='x11',MOZ_ENABLE_WAYLAND='0',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',LD_LIBRARY_PATH=str(base/'sysroot/usr/lib/x86_64-linux-gnu'),DENO_DIR='/workspace/.floorp-cloud/deno-cache')
for k in list(env):
 if k.startswith('MOZ_DISABLE_') or k=='WAYLAND_DISPLAY':env.pop(k)
owned=[];m=None;proc=None;portpath=repo/'_dist/marionette-port.txt';oldport=portpath.read_bytes() if portpath.exists() else None;link=repo/'_dist/profile/test';assert not link.exists();result={'kind':'standard top-level no-autostart collector against immutable fullproduction product','product':str(product),'sourceSha':'e55152c559a141001a375ac77098d141b0db3ca7','display':'Xvfb105 X11','input':'automation/API setup, no physical claim'}
def start(n,args):
 log=(root/(n+'.log')).open('w');p=subprocess.Popen(args,env=env,cwd=str(repo) if n=='standard-runner' else str(root),stdout=log,stderr=subprocess.STDOUT);owned.append((p,log));return p

def browser(n):
 global m,proc
 proc=start(n,[str(product/'floorp'),'--no-remote','--profile',str(profile),'--marionette','about:blank'])
 for _ in range(180):
  assert proc.poll() is None
  try:m=Marionette('127.0.0.1',2923);break
  except OSError:time.sleep(.2)
 else:raise RuntimeError('browser unavailable')
 time.sleep(4)
 return m

def quit(n):
 global m
 response=m.call('Marionette:Quit',{'flags':['eAttemptQuit']});m.socket.close();m=None;rc=proc.wait(30);save(n,{'response':response,'exitCode':rc});assert rc==0 and not response['forced']
try:
 assert not Path('/tmp/.X105-lock').exists() and not Path('/tmp/.X11-unix/X105').exists()
 x=start('xvfb',[str(base/'sysroot/usr/bin/Xvfb'),':105','-screen','0','1280x900x24','-nolisten','tcp']);time.sleep(1)
 browser('install-browser')
 install=m.async_script("const done=arguments[arguments.length-1];ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs').AddonManager.getInstallForURL(arguments[0]).then(i=>{i.addListener({onInstallEnded(_i,a){done({id:a.id,signedState:a.signedState});},onDownloadFailed(i){done({error:i.error});},onInstallFailed(i){done({error:i.error});}});i.install();});",['file://'+str(base/'branding/ja.xpi')]);save('install',install);assert install['signedState']==2
 m.script("Services.locale.requestedLocales=['ja'];return true;");quit('install-normal-quit')
 link.parent.mkdir(parents=True,exist_ok=True);link.symlink_to(profile,target_is_directory=True);portpath.write_text('2923\n')
 collector=start('standard-runner',['/workspace/.floorp-cloud/tools/node_modules/.bin/deno','task','test','--near','browser-features/modules/modules/i18n/test/LegacyBranding.test.mts','--no-autostart','--timeout-ms','15000'])
 for _ in range(100):
  if (profile/'nora-tests-control.json').exists():break
  assert collector.poll() is None,'collector ended before control ready';time.sleep(.1)
 else:raise RuntimeError('no standard runner control file')
 control=json.loads((profile/'nora-tests-control.json').read_text());save('standard-control',control)
 browser('collector-browser')
 identity=m.script("return {version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,profile:Services.dirsvc.get('ProfD',Ci.nsIFile).path,executable:Services.dirsvc.get('XREExeF',Ci.nsIFile).path,locale:Services.locale.appLocaleAsBCP47,sandbox:Services.prefs.getIntPref('security.sandbox.content.level'),sandboxDisabled:Services.env.exists('MOZ_DISABLE_CONTENT_SANDBOX'),startupMode:Services.prefs.getStringPref('nora.startup.mode','<unset>'),testState:Services.prefs.getStringPref('nora.tests.state','<absent>')};");identity['localeAddons']=m.async_script("const done=arguments[arguments.length-1];ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs').AddonManager.getAddonsByTypes(['locale']).then(xs=>done(xs.map(a=>({id:a.id,signedState:a.signedState,isActive:a.isActive}))));");save('actual-identity',identity)
 assert identity['locale']=='ja' and identity['sandbox']==6 and not identity['sandboxDisabled'] and identity['profile']==str(profile)
 assert any(a['id']=='langpack-ja@firefox.mozilla.org' and a['signedState']==2 and a['isActive'] for a in identity['localeAddons'])
 result['collectorExit']=collector.wait(35);result['actualIdentity']=identity;result['finalTestState']=m.script("return Services.prefs.getStringPref('nora.tests.state','<absent>');")
 quit('probe-normal-quit');result['completed']=True
except Exception as e:result['error']=str(e);result['traceback']=traceback.format_exc()
finally:
 if m:
  try:quit('failure-normal-quit')
  except Exception:pass
 for p,l in reversed(owned):
  if p.poll() is None:
   p.terminate()
   try:p.wait(10)
   except subprocess.TimeoutExpired:p.kill();p.wait()
  l.close()
 if link.is_symlink():link.unlink()
 if oldport is None:portpath.unlink(missing_ok=True)
 else:portpath.write_bytes(oldport)
 after=inventory();save('product-after',after);result['productUnchanged']=before==after;result['ownedProcessesRemaining']=sum(p.poll() is None for p,_ in owned);result['testSourceChanged']=subprocess.check_output(['git','-C',str(repo),'status','--porcelain'],text=True).strip();save('result',result);print(json.dumps(result,indent=2))
