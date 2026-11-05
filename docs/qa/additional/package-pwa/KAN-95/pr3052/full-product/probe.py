#!/usr/bin/env python3
"""Unmodified complete-product KAN95 natural margin/density probe. No stylesheet injection."""
import os,sys,time,json,subprocess,hashlib,traceback,argparse
from pathlib import Path
from PIL import ImageGrab
sys.path.insert(0,'/workspace/qa-linux-22/package-pwa');from pwa_ui_probe import Marionette
p=argparse.ArgumentParser();p.add_argument('--product',type=Path,required=True);p.add_argument('--source-sha',required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
out=a.out.resolve();out.mkdir(exist_ok=False,parents=True);product=a.product.resolve();sysroot=Path('/workspace/qa-linux-22/package-pwa/sysroot');profile=out/'profile';profile.mkdir();home=out/'home';home.mkdir();runtime=out/'runtime';runtime.mkdir(mode=0o700)
(profile/'user.js').write_text('user_pref("marionette.port",2955);\n')
env=dict(os.environ,HOME=str(home),XDG_CONFIG_HOME=str(home/'.config'),XDG_DATA_HOME=str(home/'.local/share'),XDG_CACHE_HOME=str(home/'.cache'),XDG_RUNTIME_DIR=str(runtime),DISPLAY=':117',GDK_BACKEND='x11',MOZ_ENABLE_WAYLAND='0',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',LD_LIBRARY_PATH=str(sysroot/'usr/lib/x86_64-linux-gnu'))
for k in list(env):
 if k.startswith('MOZ_DISABLE_')or k=='WAYLAND_DISPLAY':env.pop(k)
def inv():return {str(f.relative_to(product)):hashlib.sha256(f.read_bytes()).hexdigest()for f in product.rglob('*')if f.is_file()}
def tree(files):
 value={}
 for item in sorted(product.rglob('*')):
  key=str(item.relative_to(product))
  if item.is_symlink():value[key]={'kind':'symlink','target':str(item.readlink())}
  elif item.is_dir():value[key]={'kind':'directory'}
  elif item.is_file():value[key]={'kind':'file','sha256':files[key],'size':item.stat().st_size}
 return value
before=inv();before_tree=tree(before);(out/'product-tree-before.json').write_text(json.dumps(before_tree,indent=2));(out/'product-before.json').write_text(json.dumps(before,indent=2));owned=[];logs=[];m=None;result={'product':str(product),'sourceSha':a.source_sha,'input':'synthetic Hub WebDriver clicks; genuine container service; native selected blue and clear multiselection; pointer moved off tabs, no hover coverage','requestedStateMode':'natural-margin24','cases':[]}
def save(n,v):(out/(n+'.json')).write_text(json.dumps(v,indent=2))
def start(n,cmd):
 f=(out/(n+'.log')).open('w');logs.append(f);q=subprocess.Popen(cmd,env=env,stdout=f,stderr=subprocess.STDOUT);owned.append(q);return q
def script(s,context='chrome',args=None):
 m.call('Marionette:SetContext',{'value':context});return m.script(s,args)
def wait(s,context='chrome',timeout=40):
 end=time.monotonic()+timeout;v=None
 while time.monotonic()<end:
  v=script(s,context)
  if v:return v
  time.sleep(.25)
 raise RuntimeError('readiness timeout '+str(v))
def click(sel,context='content'):
 script('return true',context);m.click(sel);time.sleep(.8)
def shot(name):
 ImageGrab.grab(xdisplay=':117').save(out/(name+'.png'))
SNAP=r'''
const props=['display','visibility','opacity','backgroundColor','backgroundImage','borderTopColor','borderBottomColor','borderLeftColor','borderRightColor','borderTopWidth','borderBottomWidth','borderLeftWidth','borderRightWidth','boxShadow','transform','height','width','content','position','zIndex','color','top','bottom','left','right','borderBottomStyle','borderTopStyle','borderLeftStyle','borderRightStyle','marginBlockStart','marginBlockEnd','paddingBlockStart','paddingBlockEnd'];
function info(e,pseudo=null){if(!e)return null;const c=getComputedStyle(e,pseudo),r=e.getBoundingClientRect();return {tag:e.localName,class:e.className,attrs:Object.fromEntries(Array.from(e.attributes,a=>[a.name,a.value])),rect:{x:r.x,y:r.y,width:r.width,height:r.height},styles:Object.fromEntries(props.map(p=>[p,c[p]])),identity:c.getPropertyValue('--identity-tab-color'),identityColor:c.getPropertyValue('--identity-icon-color'),tokens:Object.fromEntries(['--tab-inline-padding','--tab-inner-inline-margin','--tab-padding-inline','--tab-margin-inline-inner','--usercontext-line-width','--tab-block-margin','--tab-margin-block'].map(x=>[x,c.getPropertyValue(x)]))}};
return {density:Services.prefs.getIntPref('browser.uidensity'),fontsStatus:document.fonts.status,legacyRootToken:getComputedStyle(document.documentElement).getPropertyValue('--tab-block-margin'),nativeRootToken:getComputedStyle(document.documentElement).getPropertyValue('--tab-margin-block'),effectivePrefs:Object.fromEntries(['userChrome.tab.lepton_like_padding','userChrome.tab.container','userChrome.tab.photon_like_contextline','userChrome.tab.close_button_at_hover','userChrome.tab.selected_tab','userChrome.tab.container.always_long','userChrome.tab.container.on_top'].map(k=>[k,Services.prefs.getPrefType(k)===Services.prefs.PREF_BOOL?Services.prefs.getBoolPref(k):null])),tabContainerAttrs:Object.fromEntries(Array.from(gBrowser.tabContainer.attributes,a=>[a.name,a.value])),design:JSON.parse(Services.prefs.getStringPref('floorp.design.configs')).globalConfigs.userInterface,native:Services.prefs.getBoolPref('sidebar.verticalTabs'),verticalMode:gBrowser.tabContainer.verticalMode,rootAttrs:Object.fromEntries(Array.from(document.documentElement.attributes,a=>[a.name,a.value])),devicePixelRatio,screen:{x:window.mozInnerScreenX,y:window.mozInnerScreenY},tabs:Array.from(document.querySelectorAll('[data-kan95-role]'),t=>({role:t.getAttribute('data-kan95-role'),hover:t.matches(':hover'),tab:info(t),before:info(t,'::before'),after:info(t,'::after'),parts:Object.fromEntries(['.tab-context-line','.tab-background','.tab-stack','.tab-content','.tab-label-container','.tab-icon-stack','.tabbrowser-tab-background'].map(sel=>[sel,{normal:info(t.querySelector(sel)),before:info(t.querySelector(sel),'::before'),after:info(t.querySelector(sel),'::after')}]))})),sheets:Array.from(document.styleSheets,s=>({href:s.href,disabled:s.disabled}))};
'''
def snapshot(label,expected=None):
 v=script(SNAP)
 if expected is not None:v['requestedInteraction']=expected
 save(label,v);shot(label);result['cases'].append({'id':label,'design':v['design'],'verticalMode':v['verticalMode'],'evidence':[label+'.json',label+'.png']});return v
def pointer(x,y,actions=None):
 acts=[{'type':'pointerMove','duration':0,'origin':'viewport','x':int(x),'y':int(y)}]+(actions or [])
 m.call('WebDriver:PerformActions',{'actions':[{'type':'pointer','id':'qa-mouse','parameters':{'pointerType':'mouse'},'actions':acts}]});time.sleep(.5)
try:
 assert not Path('/tmp/.X117-lock').exists() and not Path('/tmp/.X11-unix/X117').exists()
 x=start('xvfb',[str(sysroot/'usr/bin/Xvfb'),':117','-screen','0','1440x1000x24','-nolisten','tcp']);
 for _ in range(60):
  assert x.poll()is None
  if Path('/tmp/.X11-unix/X117').exists():break
  time.sleep(.1)
 q=start('browser',[str(product/'floorp'),'--no-remote','--profile',str(profile),'--marionette','about:hub#/features/design'])
 for _ in range(180):
  try:m=Marionette('127.0.0.1',2955);break
  except OSError:time.sleep(.25)
 assert m
 wait("return !!window.gBrowserInit?.delayedStartupFinished&&Services.prefs.prefHasUserValue('floorp.design.configs')")
 identity=script("return {version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,executable:Services.dirsvc.get('XREExeF',Ci.nsIFile).path,profile:Services.dirsvc.get('ProfD',Ci.nsIFile).path,sandbox:Services.prefs.getIntPref('security.sandbox.content.level'),disabled:Services.env.exists('MOZ_DISABLE_CONTENT_SANDBOX'),containers:Services.prefs.getBoolPref('privacy.userContext.enabled')}");save('identity',identity);assert identity['sandbox']==6 and not identity['disabled'];assert identity['profile']==str(profile)
 wait("return !!document.querySelector('input[name=design]')",'content');save('hub-inputs',script("return Array.from(document.querySelectorAll('input'),e=>({name:e.name,value:e.value,type:e.type}))",'content'))
 tabs=script(r"""const {ContextualIdentityService:C}=ChromeUtils.importESModule('moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs');const b=C.create('QA Blue','fingerprint','blue'),o=C.create('QA Orange','briefcase','orange');window.__kan95Hub=gBrowser.selectedTab;for(const [role,id]of[['blue',b.userContextId],['orange',o.userContextId],['normal',0]]){const t=gBrowser.addTrustedTab('about:blank',{userContextId:id,inBackground:true});t.setAttribute('data-kan95-role',role);t.setAttribute('label','KAN95 '+role);}return {blue:b,orange:o};""");save('containers',tabs)
 for design in ['proton','lepton','photon','fluerial']:
  script('gBrowser.selectedTab=window.__kan95Hub;return true;');time.sleep(.5)
  click('label:has(input[name=design][value='+design+'])')
  hv=script("return {checked:document.querySelector('input[name=design]:checked')?.value};",'content');save('hub-design-'+design,hv);assert hv['checked']==design,hv
  wait("return JSON.parse(Services.prefs.getStringPref('floorp.design.configs')).globalConfigs.userInterface==="+repr(design))
  for vertical in [False,True]:
   script('gBrowser.selectedTab=window.__kan95Hub;return true;');time.sleep(.4)
   click('input[name=style][value='+('vertical'if vertical else'horizontal')+']')
   wait('return gBrowser.tabContainer.verticalMode==='+str(vertical).lower())
   for density in [0,1,2]:
    script('Services.prefs.setIntPref("browser.uidensity",arguments[0]);return true;',args=[density]);time.sleep(.8)
    script("gBrowser.clearMultiSelectedTabs();gBrowser.selectedTab=document.querySelector('[data-kan95-role=blue]');return true;");time.sleep(.6)
    pointer(1000,700)
    v=snapshot(design+'-'+('v'if vertical else'h')+'-density-'+str(density))
    assert v['density']==density and v['native']==v['verticalMode']==vertical
 assert len(result['cases'])==24,len(result['cases'])
 result['quit']=m.call('Marionette:Quit',{'flags':['eAttemptQuit']});m.socket.close();m=None;result['browserExit']=q.wait(timeout=40);assert result['browserExit']==0
 result['completed']=True
except Exception as e:result['error']=str(e);result['traceback']=traceback.format_exc()
finally:
 if m:
  try:result['failureQuit']=m.call('Marionette:Quit',{'flags':['eAttemptQuit']});m.socket.close();owned[-1].wait(timeout=30)
  except Exception:pass
 for q in reversed(owned):
  if q.poll()is None:
   q.terminate()
   try:q.wait(timeout=15)
   except subprocess.TimeoutExpired:q.kill();q.wait()
 for f in logs:f.close()
 after=inv();after_tree=tree(after);save('product-tree-after',after_tree);result['productTreeUnchanged']=before_tree==after_tree;save('product-after',after);result['productUnchanged']=before==after;result['ownedProcessesRemaining']=sum(q.poll()is None for q in owned);result['displaySocketAbsent']=not Path('/tmp/.X11-unix/X117').exists();save('result',result);print(json.dumps({k:v for k,v in result.items()if k!='cases'},indent=2))
