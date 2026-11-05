#!/usr/bin/env python3
"""Unmodified complete-product non-Photon container rendering probe."""
import os,sys,time,json,subprocess,hashlib,traceback,argparse
from pathlib import Path
from PIL import ImageGrab
sys.path.insert(0,'/workspace/qa-linux-22/package-pwa');from pwa_ui_probe import Marionette
p=argparse.ArgumentParser();p.add_argument('--product',type=Path,default=Path('/workspace/qa-linux-additional/KAN-24/followup/products/baseline/floorp'));p.add_argument('--source-sha',default='29a3eb23145da90706d8a94441373278521cc737');p.add_argument('--out',type=Path,default=Path(__file__).parent/'run');modes=p.add_mutually_exclusive_group();modes.add_argument('--extra-hover-only',action='store_true');modes.add_argument('--all-states',action='store_true');a=p.parse_args()
out=a.out.resolve();out.mkdir(exist_ok=False,parents=True);product=a.product.resolve();sysroot=Path('/workspace/qa-linux-22/package-pwa/sysroot');profile=out/'profile';profile.mkdir();home=out/'home';home.mkdir();runtime=out/'runtime';runtime.mkdir(mode=0o700)
(profile/'user.js').write_text('user_pref("marionette.port",2930);\n')
env=dict(os.environ,HOME=str(home),XDG_CONFIG_HOME=str(home/'.config'),XDG_DATA_HOME=str(home/'.local/share'),XDG_CACHE_HOME=str(home/'.cache'),XDG_RUNTIME_DIR=str(runtime),DISPLAY=':107',GDK_BACKEND='x11',MOZ_ENABLE_WAYLAND='0',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',LD_LIBRARY_PATH=str(sysroot/'usr/lib/x86_64-linux-gnu'))
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
before=inv();before_tree=tree(before);(out/'product-tree-before.json').write_text(json.dumps(before_tree,indent=2));(out/'product-before.json').write_text(json.dumps(before,indent=2));owned=[];logs=[];m=None;result={'product':str(product),'sourceSha':a.source_sha,'input':'synthetic Hub WebDriver clicks; genuine container service; native tabbrowser API selection/multiselection; real WebDriver pointer hover','requestedStateMode':'extra-hover-only' if a.extra_hover_only else 'all-states' if a.all_states else 'original40','cases':[]}
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
 ImageGrab.grab(xdisplay=':107').save(out/(name+'.png'))
SNAP=r'''
const props=['display','visibility','opacity','backgroundColor','backgroundImage','borderTopColor','borderBottomColor','borderLeftColor','borderRightColor','borderTopWidth','borderBottomWidth','borderLeftWidth','borderRightWidth','boxShadow','transform','height','width','content','position','zIndex','color','top','bottom','left','right','borderBottomStyle','borderTopStyle','borderLeftStyle','borderRightStyle'];
function info(e,pseudo=null){if(!e)return null;const c=getComputedStyle(e,pseudo),r=e.getBoundingClientRect();return {tag:e.localName,class:e.className,attrs:Object.fromEntries(Array.from(e.attributes,a=>[a.name,a.value])),rect:{x:r.x,y:r.y,width:r.width,height:r.height},styles:Object.fromEntries(props.map(p=>[p,c[p]])),identity:c.getPropertyValue('--identity-tab-color'),identityColor:c.getPropertyValue('--identity-icon-color'),tokens:Object.fromEntries(['--tab-inline-padding','--tab-inner-inline-margin','--tab-padding-inline','--tab-margin-inline-inner','--usercontext-line-width'].map(x=>[x,c.getPropertyValue(x)]))}};
return {effectivePrefs:Object.fromEntries(['userChrome.tab.container','userChrome.tab.photon_like_contextline','userChrome.tab.close_button_at_hover','userChrome.tab.selected_tab','userChrome.tab.container.always_long','userChrome.tab.container.on_top'].map(k=>[k,Services.prefs.getPrefType(k)===Services.prefs.PREF_BOOL?Services.prefs.getBoolPref(k):null])),tabContainerAttrs:Object.fromEntries(Array.from(gBrowser.tabContainer.attributes,a=>[a.name,a.value])),design:JSON.parse(Services.prefs.getStringPref('floorp.design.configs')).globalConfigs.userInterface,native:Services.prefs.getBoolPref('sidebar.verticalTabs'),verticalMode:gBrowser.tabContainer.verticalMode,rootAttrs:Object.fromEntries(Array.from(document.documentElement.attributes,a=>[a.name,a.value])),devicePixelRatio,screen:{x:window.mozInnerScreenX,y:window.mozInnerScreenY},tabs:Array.from(document.querySelectorAll('[data-kan26-role]'),t=>({role:t.getAttribute('data-kan26-role'),hover:t.matches(':hover'),tab:info(t),before:info(t,'::before'),after:info(t,'::after'),parts:Object.fromEntries(['.tab-context-line','.tab-background','.tab-stack','.tab-content','.tab-label-container','.tab-icon-stack','.tabbrowser-tab-background'].map(sel=>[sel,{normal:info(t.querySelector(sel)),before:info(t.querySelector(sel),'::before'),after:info(t.querySelector(sel),'::after')}]))})),sheets:Array.from(document.styleSheets,s=>({href:s.href,disabled:s.disabled}))};
'''
def snapshot(label,expected=None):
 v=script(SNAP)
 if expected is not None:v['requestedInteraction']=expected
 save(label,v);shot(label);result['cases'].append({'id':label,'design':v['design'],'verticalMode':v['verticalMode'],'evidence':[label+'.json',label+'.png']});return v
def pointer(x,y,actions=None):
 acts=[{'type':'pointerMove','duration':0,'origin':'viewport','x':int(x),'y':int(y)}]+(actions or [])
 m.call('WebDriver:PerformActions',{'actions':[{'type':'pointer','id':'qa-mouse','parameters':{'pointerType':'mouse'},'actions':acts}]});time.sleep(.5)
try:
 assert not Path('/tmp/.X107-lock').exists() and not Path('/tmp/.X11-unix/X107').exists()
 x=start('xvfb',[str(sysroot/'usr/bin/Xvfb'),':107','-screen','0','1440x1000x24','-nolisten','tcp']);
 for _ in range(60):
  assert x.poll()is None
  if Path('/tmp/.X11-unix/X107').exists():break
  time.sleep(.1)
 q=start('browser',[str(product/'floorp'),'--no-remote','--profile',str(profile),'--marionette','about:hub#/features/design'])
 for _ in range(180):
  try:m=Marionette('127.0.0.1',2930);break
  except OSError:time.sleep(.25)
 assert m
 wait("return !!window.gBrowserInit?.delayedStartupFinished&&Services.prefs.prefHasUserValue('floorp.design.configs')")
 identity=script("return {version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,executable:Services.dirsvc.get('XREExeF',Ci.nsIFile).path,profile:Services.dirsvc.get('ProfD',Ci.nsIFile).path,sandbox:Services.prefs.getIntPref('security.sandbox.content.level'),disabled:Services.env.exists('MOZ_DISABLE_CONTENT_SANDBOX'),containers:Services.prefs.getBoolPref('privacy.userContext.enabled')}");save('identity',identity);assert identity['sandbox']==6 and not identity['disabled'];assert identity['profile']==str(profile)
 wait("return !!document.querySelector('input[name=design]')",'content');save('hub-inputs',script("return Array.from(document.querySelectorAll('input'),e=>({name:e.name,value:e.value,type:e.type}))",'content'))
 tabs=script(r"""const {ContextualIdentityService:C}=ChromeUtils.importESModule('moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs');const b=C.create('QA Blue','fingerprint','blue'),o=C.create('QA Orange','briefcase','orange');window.__kan26Hub=gBrowser.selectedTab;for(const [role,id]of[['blue',b.userContextId],['orange',o.userContextId],['normal',0]]){const t=gBrowser.addTrustedTab('about:blank',{userContextId:id,inBackground:true});t.setAttribute('data-kan26-role',role);t.setAttribute('label','KAN26 '+role);}return {blue:b,orange:o};""");save('containers',tabs)
 result['requestedStateMode']='bounded-normal-control-diagnostic'
 def fonts_ready():
  m.call('Marionette:SetContext',{'value':'chrome'})
  return m.async_script("const done=arguments[arguments.length-1];document.fonts.ready.then(()=>requestAnimationFrame(()=>requestAnimationFrame(()=>done({status:document.fonts.status,time:performance.now()}))));")
 def control_record(label):
  snapshot(label)
  data=script(r"""const t=document.querySelector('[data-kan26-role=normal]'),content=t.querySelector('.tab-content'),label=t.querySelector('.tab-label-container'),close=t.querySelector('.tab-close-button');const props=['display','visibility','opacity','width','height','fontFamily','fontSize','fontWeight','paddingInlineStart','paddingInlineEnd','marginInlineStart','marginInlineEnd','transitionProperty','transitionDuration','transitionDelay'];const details=(e,p)=>{let c=getComputedStyle(e,p),r=e.getBoundingClientRect();return {rect:r.toJSON(),styles:Object.fromEntries(props.map(x=>[x,c[x]]))};};const rules=(e,p)=>{try{return Array.from(InspectorUtils.getCSSStyleRules(e,p),r=>{const values=Object.fromEntries(['width','right','left','border-bottom','padding','padding-inline','margin','margin-inline','display','opacity','transition','transition-property','transition-duration'].map(k=>[k,r.style?.getPropertyValue(k)]).filter(x=>x[1]));const conditions=[];for(let p=r.parentRule;p;p=p.parentRule)if(p.conditionText)conditions.push(p.conditionText);return {selector:r.selectorText,values,conditions,href:r.parentStyleSheet?.href?.startsWith('data:')?'data:stylesheet':r.parentStyleSheet?.href};}).filter(x=>Object.keys(x.values).length)}catch(e){return {error:String(e)}}};return {nativeVersion:Services.appinfo.version,design:JSON.parse(Services.prefs.getStringPref('floorp.design.configs')).globalConfigs.userInterface,verticalMode:gBrowser.tabContainer.verticalMode,containerAttrs:Object.fromEntries(Array.from(gBrowser.tabContainer.attributes,a=>[a.name,a.value])),closebuttons:gBrowser.tabContainer.getAttribute('closebuttons'),windowRect:{x:window.screenX,y:window.screenY,width:window.outerWidth,height:window.outerHeight},devicePixelRatio,fonts:document.fonts.status,labelText:t.querySelector('.tab-label').textContent,hover:t.matches(':hover'),selected:t.hasAttribute('selected'),content:details(content),before:details(content,'::before'),label:details(label),close:details(close),matchedBeforeRules:rules(content,'::before'),matchedLabelRules:rules(label),matchedCloseRules:rules(close),animations:gBrowser.tabContainer.getAnimations({subtree:true}).map(a=>({playState:a.playState,currentTime:a.currentTime,timing:a.effect?.getTiming()})),activeElement:{tag:document.activeElement?.localName,id:document.activeElement?.id}};""")
  save(label+'-telemetry',data)
 save('initial-fonts-ready',fonts_ready())
 click('label:has(input[name=design][value=lepton])');click('input[name=style][value=vertical]');wait('return gBrowser.tabContainer.verticalMode===true')
 script("gBrowser.clearMultiSelectedTabs();gBrowser.selectedTab=document.querySelector('[data-kan26-role=orange]');gBrowser.addToMultiSelectedTabs(document.querySelector('[data-kan26-role=blue]'));gBrowser.addToMultiSelectedTabs(document.querySelector('[data-kan26-role=orange]'));return true;")
 r=script("const r=document.querySelector('[data-kan26-role=blue]').getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2};");pointer(r['x'],r['y']);time.sleep(.5)
 script('gBrowser.selectedTab=window.__kan26Hub;return true;');time.sleep(.5);click('label:has(input[name=design][value=protonfix])');click('input[name=style][value=horizontal]');wait("return !gBrowser.tabContainer.verticalMode&&JSON.parse(Services.prefs.getStringPref('floorp.design.configs')).globalConfigs.userInterface==='protonfix';")
 script("gBrowser.clearMultiSelectedTabs();gBrowser.selectedTab=document.querySelector('[data-kan26-role=normal]');return true;");time.sleep(.6);pointer(1000,700)
 for seconds in [0,0.5,2]:
  time.sleep(seconds);control_record('protonfix-normal-initial-'+str(seconds))
 save('settled-fonts-ready',fonts_ready());control_record('protonfix-normal-fonts-ready')
 # Real standard WebDriver resizing, never assign closebuttons or CSS state.
 for width in [760,640,1280]:
  m.call('Marionette:SetContext',{'value':'chrome'});value=m.call('WebDriver:SetWindowRect',{'width':width,'height':900,'x':0,'y':0});save('window-resize-'+str(width),value);time.sleep(2);save('fonts-after-resize-'+str(width),fonts_ready());pointer(1000 if width>=1000 else width-60,700);control_record('protonfix-normal-window-'+str(width))
 r=script("const r=document.querySelector('[data-kan26-role=normal]').getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2};");pointer(r['x'],r['y']);time.sleep(2);control_record('protonfix-normal-hover')
 pointer(1000,700);time.sleep(2);control_record('protonfix-normal-dehover')
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
 after=inv();after_tree=tree(after);save('product-tree-after',after_tree);result['productTreeUnchanged']=before_tree==after_tree;save('product-after',after);result['productUnchanged']=before==after;result['ownedProcessesRemaining']=sum(q.poll()is None for q in owned);result['displaySocketAbsent']=not Path('/tmp/.X11-unix/X107').exists();save('result',result);print(json.dumps({k:v for k,v in result.items()if k!='cases'},indent=2))
