#!/usr/bin/env python3
"""New COOP+COEP/container PWA actor qualification, actual product and UI.
Synthetic WebDriver input; actual container tab created by Firefox API, not an
experiment override. Never seed SSB data, disable protection or edit product.
"""
from pathlib import Path
import os,sys,json,time,subprocess,threading,http.server,hashlib,base64,shlex,traceback
BASE=Path('/workspace/qa-linux-22/package-pwa');ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(BASE));from pwa_ui_probe import Marionette,png,wait,STORE
SYS=BASE/'sysroot';PRODUCT=Path('/workspace/qa-linux-additional/KAN-24/followup/products/baseline/floorp')
class Fixture(http.server.BaseHTTPRequestHandler):
 def log_message(self,*a):pass
 def do_GET(self):
  name='Container' if self.path.startswith('/container/') else 'Default';pre='/container/' if name=='Container' else '/isolate/'
  if self.path.endswith('.webmanifest'):
   data=json.dumps({'name':'QA COOP '+name,'short_name':'COOP '+name,'start_url':pre,'scope':pre,'display':'standalone','icons':[{'src':pre+'icon.png','sizes':'192x192','type':'image/png'}]}).encode();ct='application/manifest+json'
  elif self.path.endswith('.png'):data=png(20,140,60);ct='image/png'
  else:data=(f'<!doctype html><meta charset="utf-8"><title>QA COOP {name}</title><link rel="manifest" href="{pre}app.webmanifest"><link rel="icon" href="{pre}icon.png"><h1>QA COOP {name}</h1><p id="state"></p><script>document.getElementById("state").textContent=JSON.stringify({{crossOriginIsolated,secure:isSecureContext,sab:typeof SharedArrayBuffer}})</script>').encode();ct='text/html; charset=utf-8'
  self.send_response(200);self.send_header('Content-Type',ct);self.send_header('Content-Length',str(len(data)));self.send_header('Cross-Origin-Opener-Policy','same-origin');self.send_header('Cross-Origin-Embedder-Policy','require-corp');self.send_header('Cross-Origin-Resource-Policy','same-origin');self.end_headers();self.wfile.write(data)
def inventory():return {str(p.relative_to(PRODUCT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in PRODUCT.rglob('*') if p.is_file()}
SNAP=r"""return {profile:Services.dirsvc.get('ProfD',Ci.nsIFile).path,exe:Services.dirsvc.get('XREExeF',Ci.nsIFile).path,version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,sandbox:Services.prefs.getIntPref('security.sandbox.content.level'),disabled:Services.env.exists('MOZ_DISABLE_CONTENT_SANDBOX'),windows:Array.from(Services.wm.getEnumerator(null),w=>{const b=w.gBrowser?.selectedBrowser;let actor;try{actor=b?!!b.browsingContext.currentWindowGlobal.getActor('NRProgressiveWebApp'):null}catch(e){actor=String(e)}return{url:b?.currentURI?.spec,title:w.document.title,ssbid:w.document.documentElement.getAttribute('ssbid'),taskbartab:w.document.documentElement.getAttribute('taskbartab'),floorpSsbWindow:w.floorpSsbWindow===true,tabSsb:w.gBrowser?.selectedTab?.getAttribute('floorpSSB'),remoteType:b?.remoteType,userContextId:b?.getAttribute('usercontextid'),originAttributes:b?.browsingContext.originAttributes,actor}})};"""
def save(n,d): (ROOT/(n+'.json')).write_text(json.dumps(d,indent=2,ensure_ascii=False));return d
resume='--relaunch-only' in sys.argv
root=ROOT/'run';root.mkdir(exist_ok=resume);profile=root/'profile';profile.mkdir(exist_ok=resume);home=root/'home';home.mkdir(exist_ok=resume);runtime=root/'runtime';runtime.mkdir(mode=0o700,exist_ok=resume)
(profile/'user.js').write_text('user_pref("marionette.port",2920);\n')
env=dict(os.environ,HOME=str(home),XDG_CONFIG_HOME=str(home/'.config'),XDG_DATA_HOME=str(home/'.local/share'),XDG_CACHE_HOME=str(home/'.cache'),XDG_RUNTIME_DIR=str(runtime),DISPLAY=':103',GDK_BACKEND='x11',MOZ_ENABLE_WAYLAND='0',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',LD_LIBRARY_PATH=str(SYS/'usr/lib/x86_64-linux-gnu'))
for k in list(env):
 if k.startswith('MOZ_DISABLE_') or k=='WAYLAND_DISPLAY':env.pop(k)
owned=[];m=None;browser=None;x=None;server=None;before=inventory();save('product-hashes-before',before);result={'sourceSha':'29a3eb23145da90706d8a94441373278521cc737','productIntake':'/workspace/qa-linux-additional/KAN-24/followup/intake/baseline/product-intake.json','newCondition':'COOP same-origin / COEP require-corp; real container tab; not original external sites','input':'synthetic WebDriver real product install buttons; actual Firefox container API creates source tab','steps':[],'normalQuits':[]}
def start(label,cmd):
 f=(ROOT/(label+'.log')).open('w');p=subprocess.Popen(cmd,env=env,stdout=f,stderr=subprocess.STDOUT);owned.append((p,f));return p
def connect():
 global m
 for _ in range(180):
  try:m=Marionette('127.0.0.1',2920);break
  except OSError:time.sleep(.25)
 else:raise RuntimeError('Marionette unavailable')
 wait(lambda:m.script('return !!window.gBrowserInit?.delayedStartupFinished;'))
 d=m.script(SNAP);assert d['profile']==str(profile) and Path(d['exe']).resolve()==(PRODUCT/'floorp').resolve();assert d['sandbox']==6 and not d['disabled'];assert d['version']=='159.0.1';return d
def quit(label):
 global m,browser
 q=m.call('Marionette:Quit',{'flags':['eAttemptQuit']});m.socket.close();m=None;rc=browser.wait(timeout=40);result['normalQuits'].append({'label':label,'response':q,'exitCode':rc});assert rc==0 and not q['forced']
def content():
 m.call('Marionette:SetContext',{'value':'content'});v=m.script('return {url:location.href,crossOriginIsolated,isSecureContext,sharedArrayBuffer:typeof SharedArrayBuffer,title:document.title};');m.call('Marionette:SetContext',{'value':'chrome'});return v
try:
 assert not Path('/tmp/.X103-lock').exists() and not Path('/tmp/.X11-unix/X103').exists()
 x=start('xvfb',[str(SYS/'usr/bin/Xvfb'),':103','-screen','0','1280x900x24','-nolisten','tcp']);wait(lambda:Path('/tmp/.X11-unix/X103').exists())
 server=http.server.ThreadingHTTPServer(('127.0.0.1',8789),Fixture);threading.Thread(target=server.serve_forever,daemon=True).start()
 if not resume:
  browser=start('browser-initial',[str(PRODUCT/'floorp'),'--no-remote','--profile',str(profile),'--marionette','about:blank']);save('identity',connect())
  gate=m.script("const p=ChromeUtils.importESModule('resource://noraneko/modules/pwa/PwaContainerExperiment.sys.mjs').PwaContainerExperiment;return {containerExperiment:p.isEnabled(),prefContainersEnabled:Services.prefs.getBoolPref('privacy.userContext.enabled',false)};");result['containerGate']=gate
  container=m.script("const {ContextualIdentityService}=ChromeUtils.importESModule('moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs');return ContextualIdentityService.create('QA COOP Container','fingerprint','blue');");result['containerIdentity']=container
  for label,path,ctx in [('default','/isolate/',0),('container','/container/',container['userContextId'])]:
   row={'id':label,'requestedSourceContext':ctx};result['steps'].append(row);url='http://127.0.0.1:8789'+path
   m.call("Marionette:SetContext",{"value":"content"});handlesBefore=m.call("WebDriver:GetWindowHandles");m.call("Marionette:SetContext",{"value":"chrome"})
   m.script("gBrowser.selectedTab=gBrowser.addTrustedTab(arguments[0],{userContextId:arguments[1]});return true;",[url,ctx]);wait(lambda:m.script('return gBrowser.selectedBrowser.currentURI.spec;')==url);time.sleep(3)
   m.call("Marionette:SetContext",{"value":"content"});handlesAfter=m.call("WebDriver:GetWindowHandles");newHandles=[h for h in handlesAfter if h not in handlesBefore];assert len(newHandles)==1
   m.call("WebDriver:SwitchToWindow",{"handle":newHandles[0]});m.call("Marionette:SetContext",{"value":"chrome"})
   row['before']=save(label+'-before',m.script(SNAP));row['contentBefore']=save(label+'-content-before',content());assert row['contentBefore']['crossOriginIsolated'] is True
   selected=m.script("const b=gBrowser.selectedBrowser;return {remoteType:b.remoteType,originAttributes:b.browsingContext.originAttributes,context:b.getAttribute('usercontextid'),actor:!!b.browsingContext.currentWindowGlobal.getActor('NRProgressiveWebApp')};");row['selected']=selected;assert 'webCOOP+COEP' in selected['remoteType'];assert selected['actor'];assert selected['originAttributes']['userContextId']==ctx
   wait(lambda:m.script("return !!document.getElementById('ssbPageAction')?.getBoundingClientRect().width;"));m.click('#ssbPageAction');wait(lambda:m.script("return document.getElementById('ssb-panel')?.state==='open';"));row['panelContainerVisible']=m.script("return {containerRowVisible:!!document.getElementById('ssb-container-hbox')?.getBoundingClientRect().width,installVisible:!!document.getElementById('ssb-app-install-button')?.getBoundingClientRect().width};")
   m.click('#ssb-app-install-button');wait(lambda:any(a['name']=='QA COOP '+label.title() for a in m.async_script(STORE).get('apps',[])));time.sleep(4);row['store']=save(label+'-store',m.async_script(STORE));row['after']=save(label+'-after',m.script(SNAP));(ROOT/(label+'-installed.png')).write_bytes(base64.b64decode(m.call('WebDriver:TakeScreenshot',{'full':False})['value']))
  result['consoleAfterInstall']=m.script("return Services.console.getMessageArray().map(x=>x.message).filter(x=>/PWA|NRProgressive|actor|remote.type/i.test(x)).slice(-80);")
  quit('post-install')
 else:
  previous=json.loads((ROOT/'install-and-first-relaunch.json').read_text());result['steps']=previous['steps'];result['containerGate']=previous['containerGate'];result['containerIdentity']=previous['containerIdentity'];result['installationEvidence']='install-and-first-relaunch.json';result['previousNormalQuits']=previous['normalQuits']
 result['launchers']=[]
 for p in sorted((home/'.local/share/applications').glob('floorp-*.desktop')):
  vals=dict(x.split('=',1) for x in p.read_text().splitlines() if '=' in x)
  if not vals.get('Name','').startswith('QA COOP'):continue
  command=shlex.split(vals['Exec']);assert Path(command[0]).resolve()==(PRODUCT/'floorp').resolve();row={'name':vals['Name'],'desktopSha256':hashlib.sha256(p.read_bytes()).hexdigest(),'command':command};result['launchers'].append(row)
  browser=start('relaunch-'+vals['Name'].replace(' ','-'),command+['--marionette']);row['identity']=connect();time.sleep(3);row['snapshot']=save(vals['Name'].replace(' ','-')+'-relaunch',m.script(SNAP));row['content']=save(vals['Name'].replace(' ','-')+'-relaunch-content',content());assert row['content']['crossOriginIsolated'] is True;assert any((w['taskbartab'] or w['floorpSsbWindow']) and w['actor'] is True and 'webCOOP+COEP' in (w['remoteType'] or '') for w in row['snapshot']['windows']);quit('relaunch-'+vals['Name'])
 assert len(result['launchers'])==2
 result['completed']=True
except Exception as e:
 result['completed']=False;result['error']=str(e);result['traceback']=traceback.format_exc()
 if m:
  try:save('failure-snapshot',m.script(SNAP));result['consoleFailure']=m.script('return Services.console.getMessageArray().map(x=>x.message).slice(-60);')
  except Exception as ee:result['snapshotError']=str(ee)
finally:
 if m:
  try:quit('failure-cleanup')
  except Exception as e:result['quitError']=str(e)
 if server:server.shutdown();server.server_close()
 for proc,f in reversed(owned):
  if proc.poll() is None:
   proc.terminate()
   try:proc.wait(timeout=10)
   except subprocess.TimeoutExpired:proc.kill();proc.wait()
  f.close()
 after=inventory();save('product-hashes-after',after);result['productUnchanged']=before==after;result['ownedProcessesRemaining']=sum(p.poll() is None for p,f in owned);result['displaySocketAbsent']=not Path('/tmp/.X11-unix/X103').exists();save('result',result);print(json.dumps({'completed':result.get('completed'),'error':result.get('error'),'normalQuits':len(result['normalQuits']),'productUnchanged':result['productUnchanged'],'ownedProcessesRemaining':result['ownedProcessesRemaining']}))
