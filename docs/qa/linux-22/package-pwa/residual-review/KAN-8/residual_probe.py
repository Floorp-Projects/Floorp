#!/usr/bin/env python3
"""New PR198 complete-Flatpak residual cases, not a rebuild or old path-only rerun.

Runs immutable installed launcher. HTTP payload request count proves cache-content
reuse across normal quits and permission changes. Controlled profile relocation
then exercises an existing XDG-only registry. Separate conflicting registry
fixtures exercise installed fail-closed launcher behavior without real user data.
"""
from pathlib import Path
import base64, hashlib, http.server, json, os, shutil, socket, subprocess, sys, threading, time, traceback

BASE=Path('/workspace/qa-linux-22/package-pwa'); KAN=BASE/'KAN-8'; OUT=Path(__file__).resolve().parent
SYSROOT=BASE/'sysroot'; INSTALL=KAN/'full-build-home/.local/share'; PORT=2843; HTTP_PORT=18088
sys.path.insert(0,str(BASE)); from pwa_ui_probe import Marionette
OUT.mkdir(parents=True,exist_ok=True)
PAYLOAD=b'KAN8-CACHE-CONTENT:'+b'x'*65536
requests=[]
class Handler(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  requests.append({'path':self.path,'time':time.time()})
  if self.path.startswith('/payload'):
   data=PAYLOAD;cache='public, max-age=86400, immutable';typ='text/plain'
  else:
   data=b'<!doctype html><meta charset="utf-8"><title>KAN8 Cache Fixture</title><h1>KAN8 cache content</h1><pre id="result">loading</pre><script>fetch("/payload?v=1").then(r=>r.text()).then(t=>{window.qaCache={length:t.length,prefix:t.slice(0,18),transferSize:performance.getEntriesByName(location.origin+"/payload?v=1").at(-1)?.transferSize};document.getElementById("result").textContent=JSON.stringify(window.qaCache)}).catch(e=>{window.qaError=String(e)})</script>'
   cache='no-store';typ='text/html'
  self.send_response(200);self.send_header('Content-Type',typ);self.send_header('Cache-Control',cache);self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
 def log_message(self,*args):pass

def save(name,data):
 (OUT/(name+'.json')).write_text(json.dumps(data,indent=2)+'\n');return data
def hashes(paths):
 return {str(f.relative_to(OUT)):hashlib.sha256(f.read_bytes()).hexdigest() for p in paths if p.exists() for f in p.rglob('*') if f.is_file()}
def env_for(home):
 home.mkdir(parents=True,exist_ok=True)
 e=dict(os.environ,HOME=str(home),XDG_DATA_HOME=str(INSTALL),XDG_CONFIG_HOME=str(home/'.config'),XDG_CACHE_HOME=str(home/'.cache'),XDG_RUNTIME_DIR=str(OUT/'runtime'),DISPLAY=':104',DBUS_SYSTEM_BUS_ADDRESS='unix:path='+str(OUT/'system-probe-bus'),FLATPAK_BWRAP='/usr/bin/bwrap',FLATPAK_DBUSPROXY=str(SYSROOT/'usr/bin/xdg-dbus-proxy'),LD_LIBRARY_PATH=str(SYSROOT/'usr/lib/x86_64-linux-gnu'),PATH=str(SYSROOT/'usr/bin')+':/usr/bin:/bin')
 for k in list(e):
  if k.startswith('MOZ_DISABLE_') or k in ['MOZ_APP_DATA','WAYLAND_DISPLAY']:e.pop(k,None)
 return e
def command(home_permission=False,marionette=True):
 c=['dbus-run-session','--',str(SYSROOT/'usr/bin/flatpak'),'run','--user','--env=MOZ_REMOTE_ALLOW_SYSTEM_ACCESS=1','--env=MOZ_MARIONETTE_PREF_STATE_ACROSS_RESTARTS='+json.dumps({'marionette.port':PORT})]
 if home_permission:c+=['--filesystem=home']
 c+=['one.ablaze.floorp']
 if marionette:c+=['--marionette','about:blank']
 return c

owned=[];browser=None;process=None;server=None
result={'key':'KAN-8','newConditions':['HTTP disk cache content reuse across permission transitions','existing XDG-only actual profile relocation','mixed registry fail-closed in actual installed full package'],'packageCommit':'7a3b387dc2c79a14b8f49b603b5b8d85d720fb233b325238d9d4d7a03bafde5a','manifestHead':'d9aa24d58fe633bf765721f95b4ac13132ee397e','signatureScope':'locally built unsigned full PR198 Flatpak; not Flathub distribution','systemBus':'owned incomplete test bus; not full desktop portal/system bus','physicalInput':False,'display':'Xvfb104 X11','cacheRuns':[],'conflicts':[]}
def start(name,cmd,env):
 f=(OUT/(name+'.log')).open('w');p=subprocess.Popen(cmd,env=env,stdout=f,stderr=subprocess.STDOUT);owned.append((p,f));return p
def cache_run(label,home,permission):
 global browser,process
 e=env_for(home);process=start(label,command(permission),e)
 for _ in range(220):
  assert process.poll() is None, f'Flatpak exited early {process.returncode}'
  try:browser=Marionette('127.0.0.1',PORT);break
  except OSError:time.sleep(.25)
 else:raise RuntimeError('Marionette unavailable')
 if label=='legacy-initial':browser.script('Services.prefs.setStringPref("qa.kan8.cachemarker","retained-cache-profile");return true;')
 identity=browser.script("const d=k=>Services.dirsvc.get(k,Ci.nsIFile).path;return {profile:d('ProfD'),localProfile:d('ProfLD'),marker:Services.prefs.getStringPref('qa.kan8.cachemarker','MISSING'),sandbox:Services.prefs.getIntPref('security.sandbox.content.level'),sandboxDisabled:Services.env.exists('MOZ_DISABLE_CONTENT_SANDBOX'),version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,diskCacheEnabled:Services.prefs.getBoolPref('browser.cache.disk.enable')};")
 assert identity['sandbox']==6 and not identity['sandboxDisabled'] and identity['version']=='159.0.1' and identity['diskCacheEnabled']
 before=sum(x['path'].startswith('/payload') for x in requests)
 browser.call('Marionette:SetContext',{'value':'content'});browser.call('WebDriver:Navigate',{'url':f'http://127.0.0.1:{HTTP_PORT}/index?phase={label}'})
 for _ in range(150):
  v=browser.script('const t=document.getElementById("result")?.textContent;try{return JSON.parse(t)}catch{return null}')
  if v:break
  time.sleep(.2)
 assert isinstance(v,dict) and v['length']==len(PAYLOAD),v
 image=browser.call('WebDriver:TakeScreenshot',{'full':False})['value'];(OUT/(label+'.png')).write_bytes(base64.b64decode(image))
 row={'label':label,'homePermission':permission,**identity,'payload':v,'payloadRequestsBefore':before,'payloadRequestsAfter':sum(x['path'].startswith('/payload') for x in requests)}
 browser.call('Marionette:SetContext',{'value':'chrome'});row['quit']=browser.call('Marionette:Quit',{'flags':['eAttemptQuit']});browser.socket.close();browser=None;row['exitCode']=process.wait(40);assert row['exitCode']==0 and not row['quit']['forced']
 row['cacheEntryCountAfterQuit']=len(list((Path(identity['localProfile'])/'cache2/entries').glob('*')))
 result['cacheRuns'].append(row);save('result',result);print(label,json.dumps(row),flush=True)

try:
 runtime=OUT/'runtime';runtime.mkdir(mode=0o700,exist_ok=True)
 assert not Path('/tmp/.X104-lock').exists() and not Path('/tmp/.X11-unix/X104').exists()
 e=env_for(OUT/'control-home')
 start('system-test-bus',['dbus-daemon','--session','--nofork','--address=unix:path='+str(OUT/'system-probe-bus')],e)
 x=start('xvfb',[str(SYSROOT/'usr/bin/Xvfb'),':104','-screen','0','1280x900x24','-nolisten','tcp'],e)
 for _ in range(100):
  assert x.poll() is None
  if Path('/tmp/.X11-unix/X104').exists() and (OUT/'system-probe-bus').exists():break
  time.sleep(.1)
 info=subprocess.check_output([str(SYSROOT/'usr/bin/flatpak'),'info','--user','--show-commit','one.ablaze.floorp'],env=e,text=True).strip();assert info==result['packageCommit'];result['observedCommit']=info
 deployment=Path(subprocess.check_output([str(SYSROOT/'usr/bin/flatpak'),'info','--user','--show-location','one.ablaze.floorp'],env=e,text=True).strip())
 before={str(p.relative_to(deployment)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [deployment/'files/bin/floorp',deployment/'files/bin/floorp-profile-root.sh']};result['deployedLauncherHashesBefore']=before
 assert before['files/bin/floorp']=='352e6601c6bcaed6ad19c25d68cbd873ce55a65d1b48d59f0fcb813befb48f91'
 server=http.server.ThreadingHTTPServer(('127.0.0.1',HTTP_PORT),Handler);threading.Thread(target=server.serve_forever,daemon=True).start()
 home=OUT/'cache-home'
 for label,permission in [('legacy-initial',False),('legacy-home-enabled',True),('legacy-home-disabled',False)]:cache_run(label,home,permission)
 app=home/'.var/app/one.ablaze.floorp';legacy=app/'.floorp';xdg=app/'config/floorp';assert legacy.is_dir() and not xdg.exists()
 shutil.move(str(legacy),str(xdg));result['controlledRelocation']={'from':'legacy .floorp','to':'config/floorp','when':'all browser processes normally quit','meaning':'existing XDG-only profile fixture; not automatic helper migration'}
 for label,permission in [('xdg-only',False),('xdg-home-enabled',True)]:cache_run(label,home,permission)
 for kind in ['both-registries','separate-host','partial-data']:
  h=OUT/(kind+'-home');app=h/'.var/app/one.ablaze.floorp';l=app/'.floorp';q=app/'config/floorp';(app/'config').mkdir(parents=True);l.mkdir()
  paths=[l,q,h/'.floorp'];l.joinpath('keep.txt').write_text('dedicated non-user fixture')
  if kind!='partial-data':l.joinpath('profiles.ini').write_text('[General]\nVersion=2\n')
  if kind=='both-registries':q.mkdir();q.joinpath('profiles.ini').write_text('[General]\nVersion=2\n');q.joinpath('keep.txt').write_text('second dedicated fixture')
  if kind=='separate-host':(h/'.floorp').mkdir();(h/'.floorp/profiles.ini').write_text('[General]\nVersion=2\n')
  initial=hashes(paths);p=subprocess.run(command(kind=='separate-host',False),env=env_for(h),text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=35);(OUT/(kind+'.log')).write_text(p.stdout)
  after=hashes(paths);needle={'both-registries':'two Flatpak profile registries','separate-host':'separate host profile','partial-data':'without profiles.ini'}[kind]
  row={'case':kind,'exitCode':p.returncode,'expectedDiagnostic':needle,'diagnosticPresent':needle in p.stdout,'profileFixturesUnchanged':initial==after,'fixtureHashesBefore':initial,'fixtureHashesAfter':after};assert p.returncode==1 and needle in p.stdout and initial==after,row;result['conflicts'].append(row);save('result',result);print(kind,json.dumps(row),flush=True)
 result['payloadRequests']=requests;result['cacheReuseAll']=len(result['cacheRuns'])==5 and all(r['payloadRequestsAfter']==1 for r in result['cacheRuns']);result['allMarkers']=all(r['marker']=='retained-cache-profile' for r in result['cacheRuns']);result['localCachePathStable']=len(set(r['localProfile'] for r in result['cacheRuns']))==1;result['xdgSelected']=all('/config/floorp/' in r['profile'] for r in result['cacheRuns'] if r['label'].startswith('xdg'))
 result['deployedLauncherHashesAfter']={str(p.relative_to(deployment)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [deployment/'files/bin/floorp',deployment/'files/bin/floorp-profile-root.sh']};result['deployedLauncherUnchanged']=before==result['deployedLauncherHashesAfter'];result['completed']=True
except Exception as ex:
 result['error']=str(ex);result['traceback']=traceback.format_exc();print(result['traceback'],flush=True)
finally:
 if browser:
  try:
   browser.call('Marionette:SetContext',{'value':'content'});result['failureContent']=browser.script('return {url:location.href,text:document.body?.innerText};')
  except Exception:pass
  try:result['failureQuit']=browser.call('Marionette:Quit',{'flags':['eAttemptQuit']});browser.socket.close();process.wait(35)
  except Exception as ex:result['failureCleanupError']=str(ex)
 if server:server.shutdown();server.server_close()
 for p,f in reversed(owned):
  if p.poll() is None:
   p.terminate()
   try:p.wait(15)
   except subprocess.TimeoutExpired:p.kill();p.wait()
  f.close()
 result['payloadRequests']=requests
 result['ownedProcessesRemaining']=sum(p.poll() is None for p,_ in owned);result['displayClean']=not Path('/tmp/.X11-unix/X104').exists();save('result',result)
print(json.dumps({k:v for k,v in result.items() if k not in ['cacheRuns','payloadRequests','conflicts']},indent=2));sys.exit(0 if result.get('completed') and result.get('cacheReuseAll') and result.get('allMarkers') and result.get('deployedLauncherUnchanged') else 1)
