#!/usr/bin/env python3
"""One bounded cold --safe-mode relaunch of the actual existing desktop command."""
from pathlib import Path
import os,sys,json,time,subprocess,hashlib,http.server,threading,ast,traceback,shlex,base64,socket
BASE=Path('/workspace/qa-linux-22/package-pwa');ROOT=Path(__file__).resolve().parent;SYS=BASE/'sysroot';PRODUCT=Path('/workspace/qa-linux-additional/KAN-24/followup/products/baseline/floorp')
sys.path.insert(0,str(BASE));from pwa_ui_probe import Marionette,png
source=(ROOT.parent/'probe.py').read_text();node=next(n for n in ast.parse(source).body if isinstance(n,ast.ClassDef) and n.name=='Fixture');exec(compile(ast.Module(body=[node],type_ignores=[]),'same-reviewed-coop-fixture','exec'))
run=ROOT/'run';home=run/'home';profile=run/'profile';runtime=run/'runtime';assert profile.exists()
env=dict(os.environ,HOME=str(home),XDG_CONFIG_HOME=str(home/'.config'),XDG_DATA_HOME=str(home/'.local/share'),XDG_CACHE_HOME=str(home/'.cache'),XDG_RUNTIME_DIR=str(runtime),DISPLAY=':103',GDK_BACKEND='x11',MOZ_ENABLE_WAYLAND='0',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',LD_LIBRARY_PATH=str(SYS/'usr/lib/x86_64-linux-gnu'))
for k in list(env):
 if k.startswith('MOZ_DISABLE_') or k=='WAYLAND_DISPLAY':env.pop(k)
def inventory():return {str(p.relative_to(PRODUCT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in PRODUCT.rglob('*') if p.is_file()}
def save(n,v):(ROOT/(n+'.json')).write_text(json.dumps(v,ensure_ascii=False,indent=2));return v
owned=[];m=None;server=None;browser=None;before=inventory();save('cold-product-hashes-before',before);result={'sourceSha':'29a3eb23145da90706d8a94441373278521cc737','kind':'new coldsafe-mode actualgeneratedlauncher; no repeatedinstall','normalQuits':[]}
def start(n,cmd):
 f=(ROOT/(n+'.log')).open('w');p=subprocess.Popen(cmd,env=env,stdout=f,stderr=subprocess.STDOUT);owned.append((p,f));return p
SNAP="return {inSafeMode:Services.appinfo.inSafeMode,version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,profile:Services.dirsvc.get('ProfD',Ci.nsIFile).path,sandbox:Services.prefs.getIntPref('security.sandbox.content.level'),disabled:Services.env.exists('MOZ_DISABLE_CONTENT_SANDBOX'),windows:Array.from(Services.wm.getEnumerator(null),w=>{let b=w.gBrowser?.selectedBrowser;let actor;try{actor=b?!!b.browsingContext.currentWindowGlobal.getActor('NRProgressiveWebApp'):null}catch(e){actor=String(e)}return {ready:!!w.gBrowserInit?.delayedStartupFinished,url:b?.currentURI.spec,remoteType:b?.remoteType,actor,title:w.document.title,taskbartab:w.document.documentElement.getAttribute('taskbartab'),floorpSsbWindow:w.floorpSsbWindow===true}})};"
try:
 paths=[]
 for p in (home/'.local/share/applications').glob('floorp-*.desktop'):
  vals=dict(s.split('=',1) for s in p.read_text().splitlines() if '=' in s)
  if vals.get('Name')=='QA COOP Default':paths.append((p,vals))
 assert len(paths)==1;p,vals=paths[0];original=shlex.split(vals['Exec']);assert Path(original[0]).resolve()==(PRODUCT/'floorp').resolve();command=original+['--safe-mode','--marionette'];desktopBefore=hashlib.sha256(p.read_bytes()).hexdigest();result['launcher']={'path':str(p),'sha256':desktopBefore,'originalExec':vals['Exec'],'parsedOriginal':original,'addedTestArguments':['--safe-mode','--marionette'],'actualCommand':command,'desktopFileChanged':False}
 assert not Path('/tmp/.X103-lock').exists() and not Path('/tmp/.X11-unix/X103').exists();x=start('cold-xvfb',[str(SYS/'usr/bin/Xvfb'),':103','-screen','0','1280x900x24','-nolisten','tcp'])
 for _ in range(60):
  if Path('/tmp/.X11-unix/X103').exists():break
  assert x.poll() is None;time.sleep(.2)
 server=http.server.ThreadingHTTPServer(('127.0.0.1',8789),Fixture);threading.Thread(target=server.serve_forever,daemon=True).start();browser=start('cold-browser',command)
 begin=time.monotonic();deadline=begin+45
 while time.monotonic()<deadline:
  assert browser.poll() is None,'generatedlauncher exited before connection'
  try:
   with socket.create_connection(('127.0.0.1',2921),timeout=.5):pass
   m=Marionette('127.0.0.1',2921);break
  except OSError:time.sleep(.25)
 assert m is not None,'no Marionette within bounded45s'
 states=[]
 while time.monotonic()<deadline:
  d=m.script(SNAP);states.append(d)
  if any(w['ready'] and w['url']=='http://127.0.0.1:8789/isolate/' and w['taskbartab'] and w['floorpSsbWindow'] and w['actor'] is True and 'webCOOP+COEP' in (w['remoteType'] or '') for w in d['windows']):break
  time.sleep(.3)
 else:raise RuntimeError('No ready actualCOOPPWA within45s; laststate '+json.dumps(states[-1] if states else {}))
 result['readyAfterSeconds']=round(time.monotonic()-begin,2);result['ready']=save('cold-ready',d);save('cold-readiness-observations',states)
 assert d['inSafeMode'] is True and d['sandbox']==6 and not d['disabled'] and d['profile']==str(profile)
 handles=m.call('WebDriver:GetWindowHandles');found=False
 for handle in handles:
  m.call('WebDriver:SwitchToWindow',{'handle':handle,'focus':False});mark=m.script("return {id:document.documentElement.getAttribute('taskbartab'),pwa:window.floorpSsbWindow===true};")
  if mark['id'] and mark['pwa']:found=True;break
 assert found,'actualPWA chromehandle missing'
 m.call('Marionette:SetContext',{'value':'content'});result['content']=save('cold-content',m.script('return {url:location.href,crossOriginIsolated,isSecureContext,sharedArrayBuffer:typeof SharedArrayBuffer};'));assert result['content']['crossOriginIsolated'] is True
 (ROOT/'cold-pwa.png').write_bytes(base64.b64decode(m.call('WebDriver:TakeScreenshot',{'full':False})['value']));m.call('Marionette:SetContext',{'value':'chrome'});result['console']=m.script("return Services.console.getMessageArray().map(x=>x.message).filter(x=>/PWA|actor|SSB|safe.?mode/i.test(x)).slice(-80);");result['desktopUnchanged']=desktopBefore==hashlib.sha256(p.read_bytes()).hexdigest();result['completed']=True
except Exception as e:
 result['completed']=False;result['error']=str(e);result['traceback']=traceback.format_exc()
 if m:
  try:result['failureSnapshot']=save('cold-failure-snapshot',m.script(SNAP))
  except Exception:pass
finally:
 if m:
  try:q=m.call('Marionette:Quit',{'flags':['eAttemptQuit']});m.socket.close();rc=browser.wait(timeout=40);result['normalQuits'].append({'response':q,'exitCode':rc});m=None
  except Exception as e:result['quitError']=str(e)
 if server:server.shutdown();server.server_close()
 for proc,f in reversed(owned):
  if proc.poll() is None:
   proc.terminate()
   try:proc.wait(timeout=10)
   except subprocess.TimeoutExpired:proc.kill();proc.wait()
  f.close()
 after=inventory();save('cold-product-hashes-after',after);result['productUnchanged']=before==after;result['ownedProcessesRemaining']=sum(p.poll() is None for p,f in owned);result['displaySocketAbsent']=not Path('/tmp/.X11-unix/X103').exists();save('cold-result',result);print(json.dumps({k:result.get(k) for k in ['completed','error','readyAfterSeconds','desktopUnchanged','productUnchanged','ownedProcessesRemaining']}))
