#!/usr/bin/env python3
"""Actual --safe-mode residual, no forced gate or security preference changes."""
from pathlib import Path
import os,sys,time,subprocess,json,hashlib,http.server,threading,ast,traceback,base64
from PIL import ImageGrab
BASE=Path('/workspace/qa-linux-22/package-pwa');ROOT=Path(__file__).resolve().parent;SYS=BASE/'sysroot';PRODUCT=Path('/workspace/qa-linux-additional/KAN-24/followup/products/baseline/floorp')
sys.path.insert(0,str(BASE));from pwa_ui_probe import Marionette,png,wait,STORE
# Reuse exactly the already reviewed COOP/COEP fixture class without executing its runner.
source=(ROOT.parent/'probe.py').read_text();node=next(n for n in ast.parse(source).body if isinstance(n,ast.ClassDef) and n.name=='Fixture');exec(compile(ast.Module(body=[node],type_ignores=[]),'same-coop-fixture','exec'))
run=ROOT/'run';run.mkdir();home=run/'home';home.mkdir();profile=run/'profile';profile.mkdir();runtime=run/'runtime';runtime.mkdir(mode=0o700);(profile/'user.js').write_text('user_pref("marionette.port",2921);\n')
env=dict(os.environ,HOME=str(home),XDG_CONFIG_HOME=str(home/'.config'),XDG_DATA_HOME=str(home/'.local/share'),XDG_CACHE_HOME=str(home/'.cache'),XDG_RUNTIME_DIR=str(runtime),DISPLAY=':103',GDK_BACKEND='x11',MOZ_ENABLE_WAYLAND='0',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',LD_LIBRARY_PATH=str(SYS/'usr/lib/x86_64-linux-gnu'))
for k in list(env):
 if k.startswith('MOZ_DISABLE_') or k=='WAYLAND_DISPLAY':env.pop(k)
def inventory():return {str(p.relative_to(PRODUCT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in PRODUCT.rglob('*') if p.is_file()}
def save(n,v):(ROOT/(n+'.json')).write_text(json.dumps(v,ensure_ascii=False,indent=2));return v
owned=[];m=None;server=None;browser=None;before=inventory();save('product-hashes-before',before);result={'sourceSha':'29a3eb23145da90706d8a94441373278521cc737','safeModeCommandLine':True,'input':'actual safe-mode CLI; real WebDriver install button; dialog action recorded separately','normalQuits':[]}
def start(n,cmd):
 f=(ROOT/(n+'.log')).open('w');p=subprocess.Popen(cmd,env=env,stdout=f,stderr=subprocess.STDOUT);owned.append((p,f));return p
def content():
 m.call('Marionette:SetContext',{'value':'content'});v=m.script('return {url:location.href,crossOriginIsolated,isSecureContext,sharedArrayBuffer:typeof SharedArrayBuffer};');m.call('Marionette:SetContext',{'value':'chrome'});return v
SNAP="return {inSafeMode:Services.appinfo.inSafeMode,version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,profile:Services.dirsvc.get('ProfD',Ci.nsIFile).path,sandbox:Services.prefs.getIntPref('security.sandbox.content.level'),sandboxDisabled:Services.env.exists('MOZ_DISABLE_CONTENT_SANDBOX'),windows:Array.from(Services.wm.getEnumerator(null),w=>{let b=w.gBrowser?.selectedBrowser;let actor;try{actor=b?!!b.browsingContext.currentWindowGlobal.getActor('NRProgressiveWebApp'):null}catch(e){actor=String(e)}return {url:b?.currentURI.spec,remoteType:b?.remoteType,actor,title:w.document.title,taskbartab:w.document.documentElement.getAttribute('taskbartab'),floorpSsbWindow:w.floorpSsbWindow===true,installButton:!!w.document.getElementById('ssbPageAction')?.getBoundingClientRect().width}})};"
try:
 assert not Path('/tmp/.X103-lock').exists() and not Path('/tmp/.X11-unix/X103').exists()
 x=start('xvfb',[str(SYS/'usr/bin/Xvfb'),':103','-screen','0','1280x900x24','-nolisten','tcp']);wait(lambda:Path('/tmp/.X11-unix/X103').exists())
 server=http.server.ThreadingHTTPServer(('127.0.0.1',8789),Fixture);threading.Thread(target=server.serve_forever,daemon=True).start()
 browser=start('browser',[str(PRODUCT/'floorp'),'--no-remote','--profile',str(profile),'--safe-mode','--marionette','about:blank']);time.sleep(8)
 ImageGrab.grab(xdisplay=':103').save(ROOT/'startup-screen.png')
 tree=subprocess.run([str(SYS/'usr/bin/xwininfo'),'-root','-tree'],env=env,text=True,capture_output=True);(ROOT/'startup-windows.txt').write_text(tree.stdout+tree.stderr)
 (ROOT/'await-dialog-command').write_text('ready')
 command=ROOT/'dialog-command.json'
 for _ in range(600):
  if command.exists():break
  if browser.poll() is not None:raise RuntimeError('Browser exited awaiting dialog observation')
  time.sleep(.2)
 else:raise RuntimeError('No explicit observed dialog command')
 c=json.loads(command.read_text());result['observedDialogAction']=c
 if c.get('click'):
  x,y=c['click'];subprocess.run([str(SYS/'usr/bin/xdotool'),'mousemove','--sync',str(x),str(y),'click','1'],env=env,check=True)
 time.sleep(3)
 for _ in range(180):
  try:m=Marionette('127.0.0.1',2921);break
  except OSError:time.sleep(.25)
 else:raise RuntimeError('Marionette unavailable after safe-mode dialog')
 wait(lambda:m.script('return !!window.gBrowserInit?.delayedStartupFinished;'))
 identity=save('identity',m.script(SNAP));assert identity['inSafeMode'] is True;assert identity['sandbox']==6 and not identity['sandboxDisabled'];assert identity['profile']==str(profile)
 m.call('Marionette:SetContext',{'value':'content'});m.call('WebDriver:Navigate',{'url':'http://127.0.0.1:8789/isolate/'});time.sleep(3);m.call('Marionette:SetContext',{'value':'chrome'})
 result['before']=save('before-install',m.script(SNAP));result['contentBefore']=save('content-before',content());assert result['contentBefore']['crossOriginIsolated'] is True
 if result['before']['windows'][0]['installButton']:
  m.click('#ssbPageAction');wait(lambda:m.script("return document.getElementById('ssb-panel')?.state==='open';"));m.click('#ssb-app-install-button');wait(lambda:any(a['name']=='QA COOP Default' for a in m.async_script(STORE).get('apps',[])));time.sleep(3);result['store']=save('store',m.async_script(STORE));result['after']=save('after-install',m.script(SNAP));(ROOT/'after-install.png').write_bytes(base64.b64decode(m.call('WebDriver:TakeScreenshot',{'full':False})['value']));result['uiInstallSucceeded']=True
 else:result['uiInstallSucceeded']=False;result['uiCapability']='actual PWA entrypoint absent in safe mode; no bypass attempted'
 result['console']=m.script("return Services.console.getMessageArray().map(x=>x.message).filter(x=>/PWA|actor|safe.?mode|SSB/i.test(x)).slice(-80);");result['completed']=True
except Exception as e:
 result['completed']=False;result['error']=str(e);result['traceback']=traceback.format_exc()
 if m:
  try:result['failureSnapshot']=save('failure-snapshot',m.script(SNAP))
  except Exception:pass
finally:
 if m:
  try:q=m.call('Marionette:Quit',{'flags':['eAttemptQuit']});m.socket.close();rc=browser.wait(timeout=40);result['normalQuits'].append({'response':q,'exitCode':rc});m=None
  except Exception as e:result['quitError']=str(e)
 if server:server.shutdown();server.server_close()
 for p,f in reversed(owned):
  if p.poll() is None:
   p.terminate()
   try:p.wait(timeout=10)
   except subprocess.TimeoutExpired:p.kill();p.wait()
  f.close()
 after=inventory();save('product-hashes-after',after);result['productUnchanged']=before==after;result['ownedProcessesRemaining']=sum(p.poll() is None for p,f in owned);result['displaySocketAbsent']=not Path('/tmp/.X11-unix/X103').exists();save('result',result);print(json.dumps({k:result.get(k) for k in ['completed','error','uiInstallSucceeded','productUnchanged','ownedProcessesRemaining']}))
