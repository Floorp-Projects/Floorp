#!/usr/bin/env python3
"""Run exact source module and repository assertion runner in an actual product.
Xvfb is not Wayland or a physical-input test. No product mutation or mock APIs.
Invoke inside a dedicated dbus-run-session.
"""
from pathlib import Path
import argparse,os,sys,json,subprocess,socket,time,hashlib,traceback
p=argparse.ArgumentParser();p.add_argument('--product',type=Path,required=True);a=p.parse_args()
out=Path(__file__).resolve().parent;base=Path('/workspace/qa-linux-22/package-pwa');root=base/'sysroot';product=a.product.resolve(strict=True)
sys.path.insert(0,str(base));from pwa_ui_probe import Marionette
assert not Path('/tmp/.X112-lock').exists() and not Path('/tmp/.X11-unix/X112').exists()
home=out/'home-direct';profile=home/'profile';profile.mkdir(parents=True,exist_ok=False);runtime=out/'runtime-direct';runtime.mkdir();runtime.chmod(0o700)
(profile/'user.js').write_text('user_pref("marionette.port", 2994);\nuser_pref("browser.shell.checkDefaultBrowser", false);\n')
env=dict(os.environ,HOME=str(home),XDG_CONFIG_HOME=str(home/'.config'),XDG_DATA_HOME=str(home/'.local/share'),XDG_CACHE_HOME=str(home/'.cache'),XDG_RUNTIME_DIR=str(runtime),DISPLAY=':112',GDK_BACKEND='x11',MOZ_ENABLE_WAYLAND='0',LD_LIBRARY_PATH=str(root/'usr/lib/x86_64-linux-gnu'),MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',MOZ_MARIONETTE='1',MOZ_MARIONETTE_PREF_STATE_ACROSS_RESTARTS=json.dumps({'marionette.port':2994}))
for k in ['MOZ_DISABLE_CONTENT_SANDBOX','WAYLAND_DISPLAY']:env.pop(k,None)
def inventory():return {str(f.relative_to(product)):hashlib.sha256(f.read_bytes()).hexdigest() for f in product.rglob('*') if f.is_file()}
before=inventory();(out/'product-hashes-before.json').write_text(json.dumps(before,indent=2))
owned=[];client=None;result={'kind':'Actual vendored CSS+compat browser test fixture; not full-product fix qualification','display':':112 Xvfb X11','product':str(product),'profile':str(profile)}
def start(label,cmd):
 log=(out/(label+'.log')).open('w');proc=subprocess.Popen(cmd,env=env,stdout=log,stderr=subprocess.STDOUT);owned.append((proc,log));return proc
try:
 x=start('xvfb',[str(root/'usr/bin/Xvfb'),':112','-screen','0','1280x900x24','-nolisten','tcp'])
 for _ in range(60):
  if Path('/tmp/.X11-unix/X112').exists():break
  assert x.poll() is None
  time.sleep(.2)
 proc=start('browser',[str(product/'floorp'),'--no-remote','--profile',str(profile),'--marionette','about:blank'])
 for _ in range(180):
  try:client=Marionette('127.0.0.1',2994);break
  except OSError:time.sleep(.25)
 assert client is not None,'Marionette unavailable'
 actual=client.script("return {profile:Services.dirsvc.get('ProfD',Ci.nsIFile).path,executable:Services.dirsvc.get('XREExeF',Ci.nsIFile).path,version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,sandbox:Services.prefs.getIntPref('security.sandbox.content.level'),sandboxDisable:Services.env.exists('MOZ_DISABLE_CONTENT_SANDBOX')};")
 result['identity']=actual;assert actual['profile']==str(profile);assert Path(actual['executable']).resolve()==(product/'floorp').resolve();assert actual['sandbox']==6 and not actual['sandboxDisable']
 client.call('WebDriver:SetTimeouts',{'script':180000});client.socket.settimeout(200)
 client.script("globalThis.InspectorUtils=window.InspectorUtils;return typeof InspectorUtils;");result['variants']={}
 time.sleep(5)
 for variant in ['baseline','candidate']:
  bundle=(out/(variant+'-test.js')).read_text();cases='qaFluerialContainer.runAllTests()'
  expression="const done=arguments[arguments.length-1];"+bundle+"\n"+cases+".then(value=>done(value)).catch(e=>done({error:String(e),stack:e.stack}));"
  result['variants'][variant]=client.async_script(expression)
 result['quit']=client.call('Marionette:Quit',{'flags':['eAttemptQuit']});client.socket.close();client=None;result['browserExit']=proc.wait(timeout=40)
except Exception as e:result['error']=str(e);result['traceback']=traceback.format_exc()
finally:
 if client:client.socket.close()
 for proc,log in reversed(owned):
  if proc.poll() is None:
   proc.terminate()
   try:proc.wait(timeout=15)
   except subprocess.TimeoutExpired:proc.kill();proc.wait()
  log.close()
 result['ownedProcessesRemaining']=sum(proc.poll() is None for proc,_ in owned)
 result['allProductFilesUnchanged']=before==inventory()
 (out/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
