#!/usr/bin/env python3
"""Regression probe: run on Windows with an isolated Floorp binary and pass the source CSS via --css. Requires a free Marionette port. Reads the CSS into the browser; this does not test packaging."""
import argparse, base64, json, os, pathlib, platform, socket, subprocess, sys, tempfile, time
p=argparse.ArgumentParser(); p.add_argument('--css', required=True); p.add_argument('--binary', required=True); p.add_argument('--port', type=int, default=39242); p.add_argument('--out-dir', default=None); a=p.parse_args()
if platform.system()!='Windows': raise SystemExit('Requires Windows')
if not pathlib.Path(a.binary).is_file(): raise SystemExit('Binary is missing')
s=socket.socket()
try: s.bind(('127.0.0.1',a.port))
except OSError: raise SystemExit('Port already in use; select another; do not stop existing work')
finally: s.close()
if a.out_dir:
 out=pathlib.Path(a.out_dir); out.mkdir(parents=True,exist_ok=False)
else: out=pathlib.Path(tempfile.mkdtemp(prefix='kan28-evidence-'))
profile=pathlib.Path(tempfile.mkdtemp(prefix='kan28-profile-'))
(profile/'user.js').write_text('\n'.join(['user_pref("marionette.enabled", true);',f'user_pref("marionette.port", {a.port});','user_pref("browser.shell.checkDefaultBrowser", false);','user_pref("browser.startup.page", 0);','user_pref("sidebar.verticalTabs", false);','user_pref("sidebar.revamp", true);','user_pref("sidebar.visibility", "always-show");'])+'\n')
proc=subprocess.Popen([a.binary,'-no-remote','-profile',str(profile),'-marionette','--remote-allow-system-access','about:blank'],env={**os.environ,'MOZ_REMOTE_ALLOW_SYSTEM_ACCESS':'1'})
sock=None; seq=0; report={'classification':'inconclusive','os':platform.platform(),'binary':a.binary,'profile':str(profile),'screenshots':[],'remarks':['headful Gecko chrome screenshot and WebDriver click, not physical desktop input','focused view equivalence to original report is unverified','installed binary/source SHA correspondence unverified']}
def recv():
 h=b''
 while not h.endswith(b':'):
  b=sock.recv(1)
  if not b: raise RuntimeError('socket closed')
  h+=b
 n=int(h[:-1]); data=b''
 while len(data)<n:
  b=sock.recv(n-len(data))
  if not b: raise RuntimeError('short response')
  data+=b
 return json.loads(data)
def call(name,params=None):
 global seq
 seq+=1; b=json.dumps([0,seq,name,params or {}]).encode();sock.sendall(str(len(b)).encode()+b':'+b);r=recv()
 if r[2]: raise RuntimeError(str(r[2]))
 return r[3]
def js(src):
 r=call('WebDriver:ExecuteScript',{'script':src,'args':[],'newSandbox':True});return r.get('value',r)
def shot(name):
 r=call('WebDriver:TakeScreenshot',{'id':None,'full':True,'hash':False,'scroll':True}); encoded=r.get('value',r)
 b=base64.b64decode(encoded,validate=True)
 if not b.startswith(b'\x89PNG\r\n\x1a\n'): raise RuntimeError('Invalid screenshot PNG')
 path=out/(name+'.png');path.write_bytes(b);report['screenshots'].append({'path':str(path),'bytes':len(b)})
def click(selector):
 r=call('WebDriver:FindElement',{'using':'css selector','value':selector}); obj=r.get('value',r)
 elem=obj.get('element-6066-11e4-a52e-4f735466cecf') or obj.get('ELEMENT')
 if not elem: raise RuntimeError('Missing tab element')
 call('WebDriver:ElementClick',{'id':elem})
try:
 for _ in range(120):
  try: sock=socket.create_connection(('127.0.0.1',a.port),timeout=1);break
  except OSError:
   if proc.poll() is not None: raise RuntimeError('owned process exited '+str(proc.returncode))
   time.sleep(1)
 if sock is None: raise RuntimeError('Marionette timeout')
 sock.settimeout(45);recv();report['session']=call('WebDriver:NewSession',{'capabilities':{}});call('Marionette:SetContext',{'value':'chrome'})
 for _ in range(60):
  if js("return !!(window.gBrowser && window.gBrowserInit?.delayedStartupFinished);"):break
  time.sleep(1)
 else: raise RuntimeError('browser startup timeout')
 js("const p=Services.scriptSecurityManager.getSystemPrincipal(); window.k28tabs=Array.from({length:40},(_,i)=>{let t=gBrowser.addTab('about:blank',{triggeringPrincipal:p});t.setAttribute('data-k28',String(i));return t});gBrowser.selectedTab=window.k28tabs[0];return true;")
 time.sleep(4)
 def measure():
  return js("let a=gBrowser.tabContainer.arrowScrollbox, s=a?.scrollbox ?? a?.shadowRoot?.querySelector('[part=scrollbox]'); let r=s?.getBoundingClientRect();return {vertical:Services.prefs.getBoolPref('sidebar.verticalTabs',false),a:!!a,scroller:!!s,client:s?.clientWidth,total:s?.scrollWidth,left:s?.scrollLeft,rect:r?{x:r.x,y:r.y,w:r.width,h:r.height}:null,width:window.k28tabs[8]?.getBoundingClientRect().width,splitWidths:[10,11].map(i=>window.k28tabs[i]?.getBoundingClientRect().width),split:gBrowser.activeSplitView?.tabs?.length || 0};")
 def wheel(label):
  v=measure(); print(label+' before',json.dumps(v),flush=True)
  if not v['scroller'] or v['total']<=v['client']: raise AssertionError('no actual overflow '+str(v))
  s=js("let s=gBrowser.tabContainer.arrowScrollbox.scrollbox;s.scrollLeft=0;return true;")
  r=v['rect'];x=round(r['x']+r['w']/2);y=round(r['y']+r['h']/2)
  call('WebDriver:PerformActions',{'actions':[{'type':'wheel','id':'wheel','actions':[{'type':'scroll','origin':'viewport','x':x,'y':y,'deltaX':0,'deltaY':150,'duration':0}]}]});call('WebDriver:ReleaseActions');time.sleep(2)
  after=measure();shot(label); report[label]={'before':v,'after':after};print(label+' after',json.dumps(after),flush=True)
 wheel('before-split')
 setup=js("window.k28split=gBrowser.addTabSplitView([window.k28tabs[10],window.k28tabs[11]],{insertBefore:window.k28tabs[10]});gBrowser.selectedTab=window.k28tabs[10];return {count:window.k28split.tabs?.length};")
 report['setup']=setup;time.sleep(3);wheel('during-split')
 report['attributes']=js("return {normal:Array.from(window.k28tabs[8].attributes).map(x=>[x.name,x.value]),split:Array.from(window.k28tabs[10].attributes).map(x=>[x.name,x.value]),var:getComputedStyle(document.documentElement).getPropertyValue('--floorp-tab-min-width'),computed:getComputedStyle(window.k28tabs[8]).minWidth};")
 print('attributes',json.dumps(report['attributes']),flush=True)
 css=pathlib.Path(a.css).read_text(encoding='utf8')
 js("let s=document.createElement('style');s.id='kan28-test-override';s.textContent="+json.dumps(css)+";document.head.appendChild(s);return true;");time.sleep(2)
 observed=measure(); print('SOURCE CSS CHECK',json.dumps(observed),flush=True)
 if abs(observed['width']-76)>1: raise AssertionError('ordinary tab should keep its 76px minimum; got '+str(observed['width']))
 if any(abs(x-79)>1 for x in observed['splitWidths']): raise AssertionError('split tabs changed unexpectedly: '+str(observed['splitWidths']))
 wheel('candidate-rule')

 js("gBrowser.selectedTab=window.k28tabs[0];return true;");time.sleep(2);wheel('normal-with-split-existing')
 report['classification']='scroll measurements only; compare screenshots and validate browser input'

except Exception as e: report['error']=repr(e)
finally:
 if sock:
  try: call('Marionette:Quit',{'flags':['eAttemptQuit']})
  except Exception as e: report['quit_error']=repr(e)
  sock.close()
 try: proc.wait(timeout=30)
 except subprocess.TimeoutExpired:
  proc.terminate()
  try: proc.wait(timeout=15)
  except subprocess.TimeoutExpired:report['cleanup']='owned browser remains; inspect PID '+str(proc.pid)
 report['exit']=proc.poll();(out/'result.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n',encoding='utf8');print(json.dumps(report,ensure_ascii=False))
if 'error' in report:sys.exit(2)


