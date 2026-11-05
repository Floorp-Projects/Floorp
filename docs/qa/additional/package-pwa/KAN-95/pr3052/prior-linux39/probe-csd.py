from pathlib import Path
import sys,json,time,base64,subprocess,os,io
from marionette_driver.marionette import Marionette
from PIL import Image
R=Path(__file__).parent;port=int(sys.argv[1]);product=sys.argv[2];m=Marionette('localhost',port=port,socket_timeout=45);m.start_session();m.set_context('chrome')
def js(s,a=None):return m.execute_script(s,script_args=a or [])
def clickPoint(x,y,button=0):m._send_message('WebDriver:PerformActions',{'actions':[{'type':'pointer','id':'ui','parameters':{'pointerType':'mouse'},'actions':[{'type':'pointerMove','origin':'viewport','x':int(x),'y':int(y),'duration':80},{'type':'pointerDown','button':button},{'type':'pointerUp','button':button}]}]})
def visibleClick(expr):
 q=js('const e='+expr+';if(!e)throw Error("missing UI target");e.scrollIntoView({block:"center"});const r=e.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2,width:r.width,height:r.height,text:e.innerText||e.label||e.id}');assert q['width']>20 and q['height']>10,q;clickPoint(q['x'],q['y']);time.sleep(.6);return q
r={'sourceSha':'29a3eb23145da90706d8a94441373278521cc737','key':'KAN-95','github':2670,'environment':'Debian13.6 X11 Xvfb101+Openbox; XDG_CURRENT_DESKTOP=openbox actual WM identity enables native GTK CSD; not Windows10','observations':[],'uiOperations':[]}
try:
 r['product']=js('return {name:Services.appinfo.name,version:Services.appinfo.version,build:Services.appinfo.appBuildID,os:Services.appinfo.OS,signatureRequired:Services.prefs.getBoolPref("xpinstall.signatures.required"),signatureUser:Services.prefs.prefHasUserValue("xpinstall.signatures.required"),sandbox:Services.prefs.getIntPref("security.sandbox.content.level"),focusTestMode:Services.prefs.getBoolPref("focusmanager.testmode")}')
 r['graphics']=m.execute_async_script('const done=arguments[arguments.length-1];ChromeUtils.importESModule("resource://gre/modules/Troubleshoot.sys.mjs").Troubleshoot.snapshot().then(s=>done({protocol:s.graphics.windowProtocol,compositor:s.graphics.windowLayerManagerType}));')
 def capture(theme,config,mode):
  m.set_context('chrome');s=js('''return {screen:{screenX,screenY,mozInnerScreenX,mozInnerScreenY,outerWidth,outerHeight,innerWidth,innerHeight,windowState,sizemode:document.documentElement.getAttribute('sizemode'),customtitlebar:document.documentElement.hasAttribute('customtitlebar'),tabsintitlebar:document.documentElement.hasAttribute('tabsintitlebar')},prefs:['browser.tabs.inTitlebar','userChrome.padding.drag_space','userChrome.padding.drag_space.maximized'].map(name=>({name,type:Services.prefs.getPrefType(name),user:Services.prefs.prefHasUserValue(name),value:Services.prefs.getPrefType(name)===64?Services.prefs.getIntPref(name):Services.prefs.getPrefType(name)===128?Services.prefs.getBoolPref(name):null})),elements:['#navigator-toolbox','#toolbar-menubar','#TabsToolbar','#TabsToolbar > .toolbar-items','#tabbrowser-tabs','.tabbrowser-tab[selected]','.tabbrowser-tab[selected] .tab-background'].map(sel=>{let e=document.querySelector(sel);if(!e)return{selector:sel,absent:true};let c=getComputedStyle(e);return{selector:sel,rect:e.getBoundingClientRect().toJSON(),css:{display:c.display,visibility:c.visibility,paddingTop:c.paddingTop,marginTop:c.marginTop,borderTop:c.borderTopWidth,background:c.backgroundColor},vars:{tabMargin:c.getPropertyValue('--tab-block-margin'),dragSpace:c.getPropertyValue('--uc-space-above-tabbar')}}})}''')
  # EWMH state is independent of Gecko sizemode.
  wid=subprocess.check_output(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool','getactivewindow'],env=os.environ,text=True).strip()
  try:s['ewmh']=subprocess.check_output(['xprop','-id',wid,'_NET_WM_STATE','_NET_FRAME_EXTENTS'],text=True,env=os.environ).strip()
  except (OSError,subprocess.CalledProcessError) as e:s['ewmhError']=str(e)
  png=base64.b64decode(m.screenshot(format='base64'));im=Image.open(io.BytesIO(png));path=R/'screenshots'/f'{product}-{port}-{theme}-{config}-{mode}.png';im.crop((0,0,im.width,min(160,im.height))).save(path)
  s['osGeometry']=subprocess.check_output(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool','getwindowgeometry','--shell',wid],text=True,env=os.environ)
  selected=s['elements'][-1].get('rect');s['paintColumn']=[{'y':y,'rgb':list(im.convert('RGB').getpixel((int(selected['x']+selected['width']/2),y)))} for y in range(0,min(40,im.height))] if selected else []
  s['gtkCsdAvailable']=js('return matchMedia("(-moz-gtk-csd-available)").matches')
  s.update(theme=theme,configuration=config,mode=mode,screenshot=str(path.relative_to(R)));r['observations'].append(s)

  if mode=='maximized' and theme in ['Lepton','Firefox']:
   original=js('window.qaOriginal=gBrowser.selectedTab;window.qaA=gBrowser.addTab("data:text/html,<title>QA A</title>",{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()});window.qaB=gBrowser.addTab("data:text/html,<title>QA B</title>",{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()});gBrowser.selectedTab=qaA;return true');time.sleep(.5)
   q=js('let r=qaB.getBoundingClientRect();return{x:mozInnerScreenX+r.x+r.width/2,centerY:mozInnerScreenY+r.y+r.height/2,clientY:mozInnerScreenY+r.y,selected:gBrowser.selectedTab===qaB}')
   xd='/workspace/.floorp-cloud/sysroot/usr/bin/xdotool'
   subprocess.run([xd,'mousemove','--sync',str(int(q['x'])),'0','click','1'],env=os.environ,check=True);time.sleep(.3)
   selected=js('return gBrowser.selectedTab===qaB');js('gBrowser.selectedTab=qaA')
   subprocess.run([xd,'mousemove','--sync',str(int(q['x'])),str(int(q['centerY'])),'click','1'],env=os.environ,check=True);time.sleep(.3)
   r.setdefault('edgeControls',[]).append({'theme':theme,'geometry':q,'absoluteScreenTopSelected':selected,'centerSelected':js('return gBrowser.selectedTab===qaB'),'input':'OS XTest synthetic, not physical','cssFixture':False})
   js('gBrowser.selectedTab=qaOriginal;gBrowser.removeTab(qaA,{animate:false});gBrowser.removeTab(qaB,{animate:false})')
 themes=['Proton','Lepton','Fluerial'] if product=='floorp' else ['Firefox']
 for theme in themes:
  m.set_context('chrome');m.execute_script('window.restore();window.resizeTo(1200,900);window.moveTo(50,60)');time.sleep(.5)
  if theme not in ['default','Firefox']:
   m.set_context('content');m.navigate('about:hub#/features/design');time.sleep(.8)
   op=visibleClick('[...document.querySelectorAll("label")].find(e=>e.innerText.trim()===arguments[0])'.replace('arguments[0]',json.dumps(theme)));r['uiOperations'].append({'theme':theme,'visibleLabel':op})
   checked=js('return [...document.querySelectorAll("input[type=radio]")].filter(e=>e.checked).map(e=>e.value)');assert theme.lower() in checked,checked
   m.set_context('chrome');actual=js('return JSON.parse(Services.prefs.getStringPref("floorp.design.configs")).globalConfigs.userInterface');r['uiOperations'][-1]['storedDesign']=actual;assert actual==theme.lower()
  for config in ['default']:
   m.set_context('chrome')
   # Native configuration command/pref fixtures, not injected CSS.
   js('Services.prefs.setIntPref("browser.tabs.inTitlebar",arguments[0]);setToolbarVisibility(document.getElementById("toolbar-menubar"),arguments[1]);',[0 if config=='native-titlebar' else 2,config=='menubar-visible']);time.sleep(.7)
   for mode in ['normal','maximized']:
    js('window.'+('restore' if mode=='normal' else 'maximize')+'()');time.sleep(.7);capture(theme,config,mode)
 r['completed']=True
except Exception as e:
 import traceback;r.update(error=str(e),traceback=traceback.format_exc())
finally:
 (R/f'{product}-{port}.json').write_text(json.dumps(r,indent=2));print(json.dumps({'completed':r.get('completed'),'error':r.get('error'),'observations':len(r['observations']),'operations':r['uiOperations']}));m.delete_session()
