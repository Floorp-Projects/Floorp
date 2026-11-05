#!/usr/bin/env python3
"""Run exact source module and repository assertion runner in an actual product.
Xvfb is not Wayland or a physical-input test. No product mutation or mock APIs.
Invoke inside a dedicated dbus-run-session.
"""
from pathlib import Path
import argparse,os,sys,json,subprocess,socket,time,hashlib,traceback
p=argparse.ArgumentParser();p.add_argument('--product',type=Path,required=True);a=p.parse_args()
out=Path(__file__).resolve().parent;base=out.parents[1];root=base/'sysroot';product=a.product.resolve(strict=True)
sys.path.insert(0,str(base));from pwa_ui_probe import Marionette
assert not Path('/tmp/.X98-lock').exists() and not Path('/tmp/.X11-unix/X98').exists()
home=out/'home';profile=home/'profile';
import shutil
source=base/'full-build/3032/wayland-product-qa'
shutil.copytree(source/'home',home)
shutil.copytree(source/'profile',profile,ignore=shutil.ignore_patterns('lock','.parentlock','parent.lock'))
runtime=out/'runtime';runtime.mkdir();runtime.chmod(0o700)
(profile/'user.js').write_text('user_pref("marionette.port", 2898);\nuser_pref("browser.shell.checkDefaultBrowser", false);\n')
env=dict(os.environ,HOME=str(home),XDG_CONFIG_HOME=str(home/'.config'),XDG_DATA_HOME=str(home/'.local/share'),XDG_CACHE_HOME=str(home/'.cache'),XDG_RUNTIME_DIR=str(runtime),DISPLAY=':98',GDK_BACKEND='x11',MOZ_ENABLE_WAYLAND='0',LD_LIBRARY_PATH=str(root/'usr/lib/x86_64-linux-gnu'),MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',MOZ_MARIONETTE='1',MOZ_MARIONETTE_PREF_STATE_ACROSS_RESTARTS=json.dumps({'marionette.port':2898}))
for k in ['MOZ_DISABLE_CONTENT_SANDBOX','WAYLAND_DISPLAY']:env.pop(k,None)
def inventory():return {str(f.relative_to(product)):hashlib.sha256(f.read_bytes()).hexdigest() for f in product.rglob('*') if f.is_file()}
before=inventory();(out/'product-hashes-before.json').write_text(json.dumps(before,indent=2))
owned=[];client=None;result={'kind':'actual source-generated candidate copy, cold launcher native WM_CLASS smoke','display':':98 Xvfb X11 native WM_CLASS','product':str(product),'profile':str(profile)}
def start(label,cmd):
 log=(out/(label+'.log')).open('w');proc=subprocess.Popen(cmd,env=env,stdout=log,stderr=subprocess.STDOUT);owned.append((proc,log));return proc
try:
 x=start('xvfb',[str(root/'usr/bin/Xvfb'),':98','-screen','0','1280x900x24','-nolisten','tcp'])
 for _ in range(60):
  if Path('/tmp/.X11-unix/X98').exists():break
  assert x.poll() is None
  time.sleep(.2)
 sys.path.insert(0,str(out.parent))
 from probe_orders import read_entries
 entries=read_entries(home,profile,product,relocated=True)
 start('kwin',[str(root/'usr/bin/kwin_x11'),'--replace']);time.sleep(3)
 proc=start('browser',entries['alpha']['effectiveExec']+['--marionette'])
 for _ in range(180):
  try:client=Marionette('127.0.0.1',2898);break
  except OSError:time.sleep(.25)
 assert client is not None,'Marionette unavailable'
 actual=client.script("return {profile:Services.dirsvc.get('ProfD',Ci.nsIFile).path,executable:Services.dirsvc.get('XREExeF',Ci.nsIFile).path,version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,sandbox:Services.prefs.getIntPref('security.sandbox.content.level'),sandboxDisable:Services.env.exists('MOZ_DISABLE_CONTENT_SANDBOX')};")
 result['identity']=actual;assert actual['profile']==str(profile);assert Path(actual['executable']).resolve()==(product/'floorp').resolve();assert actual['sandbox']==6 and not actual['sandboxDisable']
 client.call('WebDriver:SetTimeouts',{'script':180000});client.socket.settimeout(200)
 result['stages']=[]
 for role in ['alpha','browser','beta']:
  if role=='browser':remote=start('remote-browser',[str(product/'floorp'),'--profile',str(profile),'--new-window','about:blank']);remote.wait(timeout=25)
  if role=='beta':remote=start('remote-beta',entries['beta']['effectiveExec']);remote.wait(timeout=25)
  time.sleep(3)
  clients=subprocess.run([str(root/'usr/bin/xprop'),'-root','_NET_CLIENT_LIST'],env=env,capture_output=True,text=True,check=True).stdout
  import re
  props=[]
  for xid in re.findall(r'0x[0-9a-f]+',clients):
   text=subprocess.run([str(root/'usr/bin/xprop'),'-id',xid,'WM_STATE','WM_CLASS','_NET_WM_NAME','_NET_WM_PID'],env=env,capture_output=True,text=True,check=True).stdout
   props.append({'xid':xid,'properties':text})
  result['stages'].append({'after':role,'nativeClients':props,'windows':client.script("return Array.from(Services.wm.getEnumerator('navigator:browser')).map(w=>({taskbartab:w.document.documentElement.getAttribute('taskbartab'),windowclass:w.document.documentElement.getAttribute('windowclass'),title:w.document.title}));")})
 result['expected']={k:v['startupWMClass'] for k,v in entries.items()};result['expected']['browser']='floorp'
 last=result['stages'][-1];classes=[]
 for w in last['nativeClients']:
  m=re.search(r'WM_CLASS\(STRING\) = "[^"]*", "([^"]*)"',w['properties'])
  if m:classes.append(m.group(1))
 result['nativeClassesMatch']=all(v in classes for v in result['expected'].values())
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
