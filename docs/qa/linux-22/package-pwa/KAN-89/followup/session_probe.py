#!/usr/bin/env python3
"""Independent KWin/X11 + official Flatpak icon probe; synthetic Alt+Tab.
Run inside dbus-run-session. Never changes product security or WM protocols.
"""
from pathlib import Path
import os,sys,time,json,subprocess,shutil,socket,threading,http.server,traceback,base64,re,hashlib
OUT=Path(__file__).resolve().parent;B=OUT.parents[1];ROOT=B/'sysroot';HOME=OUT/'home';RUNTIME=OUT/'runtime';PROFILE=HOME/'.var/app/one.ablaze.floorp/config/qa-profile';APP=PROFILE.parent.parent;PORT=2889
sys.path.insert(0,str(B));sys.path.insert(0,str(B/'python'));from pwa_ui_probe import Marionette
from PIL import Image,ImageGrab
for d in [HOME,RUNTIME,PROFILE]:d.mkdir(parents=True,exist_ok=True)
originalDesktop=B/'flatpak-home/.local/share/flatpak/exports/share/applications/one.ablaze.floorp.desktop'
desktopText=originalDesktop.read_text();relocated=desktopText.replace('Exec=/usr/bin/flatpak ', 'Exec='+str(ROOT/'usr/bin/flatpak')+' ')
assert [line for line in desktopText.splitlines() if not line.startswith('Exec=')]==[line for line in relocated.splitlines() if not line.startswith('Exec=')]
apps=HOME/'.local/share/applications';apps.mkdir(parents=True,exist_ok=True);(apps/'one.ablaze.floorp.desktop').write_text(relocated)
(OUT/'desktop-prefix-relocation.json').write_text(json.dumps({'originalExecExists':Path('/usr/bin/flatpak').exists(),'actualExecExists':(ROOT/'usr/bin/flatpak').exists(),'originalSha256':hashlib.sha256(desktopText.encode()).hexdigest(),'relocatedSha256':hashlib.sha256(relocated.encode()).hexdigest(),'onlyExecPathChanged':True,'StartupWMClass':'floorp','Icon':'one.ablaze.floorp','originalExecLines':[l for l in desktopText.splitlines() if l.startswith('Exec=')],'relocatedExecLines':[l for l in relocated.splitlines() if l.startswith('Exec=')]},indent=2))
RUNTIME.chmod(0o700)
subprocess.run(['cc','-O2',str(OUT/'gtk_icon_probe.c'),'-ldl','-o',str(APP/'config/gtk-icon-probe')],check=True,capture_output=True,text=True)
env=dict(os.environ,HOME=str(HOME),DISPLAY=':90',XDG_RUNTIME_DIR=str(RUNTIME),XDG_DATA_HOME=str(HOME/'.local/share'),XDG_CONFIG_HOME=str(HOME/'.config'),XDG_CACHE_HOME=str(HOME/'.cache'),XDG_DATA_DIRS=str(B/'flatpak-home/.local/share/flatpak/exports/share')+':'+str(ROOT/'usr/share')+':/usr/share',XDG_CONFIG_DIRS=str(ROOT/'etc/xdg'),QT_PLUGIN_PATH=str(ROOT/'usr/lib/x86_64-linux-gnu/qt6/plugins'),QML2_IMPORT_PATH=str(ROOT/'usr/lib/x86_64-linux-gnu/qt6/qml'),LD_LIBRARY_PATH=str(ROOT/'usr/lib/x86_64-linux-gnu'),PATH=str(ROOT/'usr/bin')+':/usr/bin:/bin',QT_QPA_PLATFORM='xcb',QT_QUICK_BACKEND='software',XDG_SESSION_TYPE='x11',XDG_CURRENT_DESKTOP='KDE',XDG_SESSION_DESKTOP='KDE',XDG_MENU_PREFIX='plasma-',KDE_FULL_SESSION='true',KDE_SESSION_VERSION='6',DESKTOP_SESSION='plasma')
for k in ['WAYLAND_DISPLAY','MOZ_DISABLE_CONTENT_SANDBOX']:env.pop(k,None)
env.update(LIBGL_DRIVERS_PATH=str(ROOT/'usr/lib/x86_64-linux-gnu/dri'),__EGL_VENDOR_LIBRARY_FILENAMES=str(ROOT/'usr/share/glvnd/egl_vendor.d/50_mesa.json'))
# Runtime may have no system bus; keep this session independent and record that limit.
fontdir=HOME/'.local/share/fonts';fontdir.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/'usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc',fontdir/'NotoSansCJK-Regular.ttc')
refs={'floorp':B/'product/floorp/browser/chrome/icons/default/default64.png','firefox':B/'branding/firefox-official64.png'}
for name,p in refs.items():shutil.copyfile(p,OUT/(name+'-reference64.png'))
theme=APP/'data/icons/qa-k89';(theme/'64x64/apps').mkdir(parents=True,exist_ok=True)
(theme/'index.theme').write_text('[Icon Theme]\nName=QA KAN89\nDirectories=64x64/apps\nInherits=hicolor\n\n[64x64/apps]\nSize=64\nType=Fixed\nContext=Applications\n')
for name,p in refs.items():
 shutil.copyfile(p,theme/('64x64/apps/'+name+'.png'));shutil.copyfile(p,APP/('config/'+name+'-reference64.png'))
shutil.copyfile(theme/'index.theme',APP/'config/qa-k89-index.theme')
gtk=APP/'config/gtk-3.0';gtk.mkdir(parents=True,exist_ok=True);(gtk/'settings.ini').write_text('[Settings]\ngtk-icon-theme-name=qa-k89\n')
for lang in ['ja','es-CL']:shutil.copyfile(B/('branding/'+lang+'.xpi'),APP/('config/'+lang+'.xpi'))
class Fixture(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  lang=self.path.strip('/') or 'en-US';text=f'<meta charset="utf-8"><title>KAN89 {lang} dedicated</title><h1>KAN89 {lang}</h1><p>日本語 Español English</p>'.encode();self.send_response(200);self.send_header('Content-Type','text/html;charset=utf-8');self.end_headers();self.wfile.write(text)
 def log_message(self,*a):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',8889),Fixture);threading.Thread(target=server.serve_forever,daemon=True).start()
owned=[];logs=[];rows=[]
def start(name,args):
 f=(OUT/(name+'.log')).open('w');logs.append(f);p=subprocess.Popen(args,env=env,stdout=f,stderr=subprocess.STDOUT);owned.append(p);return p

def run(args,**kw):return subprocess.run(args,env=env,capture_output=True,text=True,**kw)

def screenshot(name):ImageGrab.grab(xdisplay=':90').save(OUT/(name+'.png'))

try:
 x=start('xvfb',[str(ROOT/'usr/bin/Xvfb'),':90','-screen','0','1280x900x24','-nolisten','tcp'])
 for _ in range(50):
  r=run([str(ROOT/'usr/bin/xdotool'),'getdisplaygeometry'])
  if r.returncode==0:break
  time.sleep(.2)
 if x.poll() is not None:raise RuntimeError('Xvfb exited '+str(x.returncode))
 activity=start('activitymanager',[str(ROOT/'usr/lib/x86_64-linux-gnu/libexec/kactivitymanagerd')])
 accel=start('globalaccel',[str(ROOT/'usr/lib/x86_64-linux-gnu/libexec/kglobalacceld')])
 kde=start('kded6',[str(ROOT/'usr/bin/kded6')]);time.sleep(2)
 wm=start('kwin-x11',[str(ROOT/'usr/bin/kwin_x11'),'--replace'])
 for _ in range(80):
  support=run([str(ROOT/'usr/bin/xprop'),'-root','_NET_SUPPORTING_WM_CHECK','_NET_SUPPORTED'])
  if '_NET_SUPPORTING_WM_CHECK(WINDOW)' in support.stdout:break
  if wm.poll() is not None:raise RuntimeError('KWin exited '+str(wm.returncode))
  time.sleep(.2)
 (OUT/'wm-properties.txt').write_text(support.stdout+support.stderr)
 version=run([str(ROOT/'usr/bin/kwin_x11'),'--version']);(OUT/'kwin-version.txt').write_text(version.stdout+version.stderr)
 cache=run([str(ROOT/'usr/bin/kbuildsycoca6'),'--noincremental'],timeout=45);(OUT/'kbuildsycoca.log').write_text(cache.stdout+cache.stderr)
 plasma=start('plasmashell',[str(ROOT/'usr/bin/plasmashell')]);time.sleep(8)
 session=run([str(ROOT/'usr/bin/qdbus6')]);(OUT/'session-dbus-names.txt').write_text(session.stdout+session.stderr)
 control=start('control-window',[str(ROOT/'usr/bin/xmessage'),'-title','KAN89 independent control','KAN89 synthetic Alt+Tab comparison control'])
 time.sleep(1);screenshot('desktop-before-product')
 fp_env=dict(env,XDG_DATA_HOME=str(B/'flatpak-home/.local/share'),FLATPAK_BWRAP='/usr/bin/bwrap',FLATPAK_DBUSPROXY=str(ROOT/'usr/bin/xdg-dbus-proxy'))
 system_bus=start('system-probe-bus',['dbus-daemon','--session','--nofork','--address=unix:path='+str(OUT/'system-probe-bus')])
 for _ in range(50):
  if (OUT/'system-probe-bus').exists():break
  time.sleep(.1)
 fp_env['DBUS_SYSTEM_BUS_ADDRESS']='unix:path='+str(OUT/'system-probe-bus')
 flatpak=str(ROOT/'usr/bin/flatpak');base=[flatpak,'run','--user']
 installtheme=subprocess.run(base+['--command=bash','one.ablaze.floorp','-c','set -eu; case \"$XDG_DATA_HOME\" in '+str(APP/'data')+') ;; *) exit 9;; esac; mkdir -p \"$XDG_DATA_HOME/icons/qa-k89/64x64/apps\"; cp \"$XDG_CONFIG_HOME/qa-k89-index.theme\" \"$XDG_DATA_HOME/icons/qa-k89/index.theme\"; for n in floorp firefox; do cp \"$XDG_CONFIG_HOME/$n-reference64.png\" \"$XDG_DATA_HOME/icons/qa-k89/64x64/apps/$n.png\"; done; ls -l \"$XDG_DATA_HOME/icons/qa-k89/64x64/apps\"'],env=fp_env,capture_output=True,text=True,timeout=30)
 (OUT/'theme-install-sandbox.txt').write_text(installtheme.stdout+installtheme.stderr);assert installtheme.returncode==0,installtheme.stderr
 gtkprobe=subprocess.run(base+['--env=GDK_BACKEND=x11','--env=MOZ_ENABLE_WAYLAND=0','--command=bash','one.ablaze.floorp','-c','exec \"$XDG_CONFIG_HOME/gtk-icon-probe\"'],env=fp_env,capture_output=True,text=True,timeout=30)
 (OUT/'gtk-theme-probe.txt').write_text(gtkprobe.stdout+gtkprobe.stderr)
 for lang in ['en-US','ja','es-CL']:
  (PROFILE/'user.js').write_text('user_pref("marionette.port",'+str(PORT)+');\nuser_pref("intl.locale.requested",'+json.dumps(lang)+');\n')
  cmd=base+['--env=GDK_BACKEND=x11','--env=MOZ_ENABLE_WAYLAND=0','--env=MOZ_REMOTE_ALLOW_SYSTEM_ACCESS=1','--env=MOZ_MARIONETTE_PREF_STATE_ACROSS_RESTARTS='+json.dumps({'marionette.port':PORT}),'one.ablaze.floorp','--no-remote','--profile',str(PROFILE),'--marionette','http://127.0.0.1:8889/'+lang]
  cmd=[str(ROOT/'usr/bin/kstart'),'--desktopfile','one.ablaze.floorp','--','env','XDG_DATA_HOME='+str(B/'flatpak-home/.local/share'),'FLATPAK_BWRAP=/usr/bin/bwrap','FLATPAK_DBUSPROXY='+str(ROOT/'usr/bin/xdg-dbus-proxy'),'DBUS_SYSTEM_BUS_ADDRESS='+fp_env['DBUS_SYSTEM_BUS_ADDRESS']]+cmd
  row={'launchMethod':'standard KDE kstart --desktopfile one.ablaze.floorp + official Flatpak command; original desktop metadata unchanged','command':cmd,'locale':lang,'completed':False,'display':':90','windowIdentification':'dedicated fixture title + windowtree + class/PID/geometry, guarded actual ProfD','input':'synthetic xdotool Alt+Tab; not physical input'}
  log=(OUT/(lang+'-browser.log')).open('w');p=subprocess.Popen(cmd,env=env,stdout=log,stderr=subprocess.STDOUT);m=None
  try:
   for _ in range(150):
    try:m=Marionette('127.0.0.1',PORT);break
    except (ConnectionError,OSError):time.sleep(.3)
   if m is None:raise RuntimeError('Marionette unavailable')
   row['product']=m.script("const w=Services.wm.getMostRecentWindow('navigator:browser');let handle;try{handle=w.docShell.treeOwner.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIBaseWindow).nativeHandle;}catch(e){handle=String(e);}return {profile:Services.dirsvc.get('ProfD',Ci.nsIFile).path,executable:Services.dirsvc.get('XREExeF',Ci.nsIFile).path,version:Services.appinfo.version,pid:Services.appinfo.processID,locale:Services.locale.appLocaleAsBCP47,brand:Services.strings.createBundle('chrome://branding/locale/brand.properties').GetStringFromName('brandShortName'),sandbox:Services.prefs.getIntPref('security.sandbox.content.level'),sandboxDisableEnv:Services.env.exists('MOZ_DISABLE_CONTENT_SANDBOX'),startupIdPresent:Services.env.exists('DESKTOP_STARTUP_ID'),title:w.document.title,screenX:w.screenX,screenY:w.screenY,outerWidth:w.outerWidth,outerHeight:w.outerHeight,outerWindowID:String(w.windowUtils.outerWindowID),nativeHandle:handle};")
   assert row['product']['profile']==str(PROFILE),row['product'];assert row['product']['executable']=='/app/lib/floorp/floorp';assert row['product']['locale']==lang
   if lang=='en-US':
    row['installed']=[]
    for loc in ['ja','es-CL']:
     a=m.async_script("const done=arguments[arguments.length-1];ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs').AddonManager.getInstallForURL(arguments[0]).then(i=>{i.addListener({onInstallEnded(_i,a){done({id:a.id,signedState:a.signedState,active:a.isActive});},onDownloadFailed(i){done({error:i.error});},onInstallFailed(i){done({error:i.error});}});i.install();}).catch(e=>done({error:String(e)}));",['file://'+str(APP/('config/'+loc+'.xpi'))]);assert a.get('signedState')==2,a;row['installed'].append(a)
   row['packs']=m.async_script("const done=arguments[arguments.length-1];ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs').AddonManager.getAddonsByTypes(['locale']).then(xs=>done(xs.map(a=>({id:a.id,signedState:a.signedState,active:a.isActive}))));")
   time.sleep(3)
   observe=OUT/(lang+'-observe.js');observe.write_text('print(\"QA_KAN89_'+lang+'=\"+JSON.stringify(workspace.windowList().map(w=>({caption:w.caption,resourceClass:String(w.resourceClass),resourceName:String(w.resourceName),desktopFileName:String(w.desktopFileName),pid:w.pid,internalId:String(w.internalId)}))));')
   scriptname='qa-kan89-'+lang;loaded=run([str(ROOT/'usr/bin/qdbus6'),'org.kde.KWin','/Scripting','org.kde.kwin.Scripting.loadScript',str(observe),scriptname]);scriptid=loaded.stdout.strip();row['kwinScriptLoad']=loaded.stdout+loaded.stderr
   if scriptid.isdigit():
    executed=run([str(ROOT/'usr/bin/qdbus6'),'org.kde.KWin','/Scripting/Script'+scriptid,'org.kde.kwin.Script.run']);row['kwinScriptRun']=executed.stdout+executed.stderr;run([str(ROOT/'usr/bin/qdbus6'),'org.kde.KWin','/Scripting','org.kde.kwin.Scripting.unloadScript',scriptname])
   tree=run([str(ROOT/'usr/bin/xwininfo'),'-root','-tree']);(OUT/(lang+'-windowtree.txt')).write_text(tree.stdout+tree.stderr)
   found=run([str(ROOT/'usr/bin/xdotool'),'search','--name','^KAN89 '+lang+' dedicated'])
   titleIds=found.stdout.split()
   clients=run([str(ROOT/'usr/bin/xprop'),'-root','_NET_CLIENT_LIST','_NET_CLIENT_LIST_STACKING']);(OUT/(lang+'-client-list.txt')).write_text(clients.stdout+clients.stderr)
   clientRows=[];ids=[]
   for client in re.findall(r'0x[0-9a-f]+',clients.stdout.splitlines()[0]):
    props=run([str(ROOT/'usr/bin/xprop'),'-id',client,'WM_STATE','WM_CLASS','_NET_WM_NAME','WM_NAME','_NET_WM_PID','_NET_WM_ICON']);clientRows.append({'id':client,'properties':props.stdout})
    if 'KAN89 '+lang+' dedicated' in props.stdout and 'WM_STATE(WM_STATE)' in props.stdout:ids.append(client)
   (OUT/(lang+'-client-properties.json')).write_text(json.dumps(clientRows,indent=2))
   assert len(ids)==1,{'clientCandidates':ids,'titleCandidates':titleIds};wid=ids[0];row['windowID']=wid;row['titleCandidateIDs']=titleIds;row['clientListMatch']=True
   children=run([str(ROOT/'usr/bin/xwininfo'),'-id',wid,'-tree']);(OUT/(lang+'-client-tree.txt')).write_text(children.stdout+children.stderr)
   props=run([str(ROOT/'usr/bin/xprop'),'-id',wid,'WM_CLASS','_NET_WM_NAME','_NET_WM_PID','_NET_WM_ICON']);(OUT/(lang+'-xprop.txt')).write_text(props.stdout+props.stderr)
   geometry=run([str(ROOT/'usr/bin/xdotool'),'getwindowgeometry','--shell',wid]);row['geometry']=geometry.stdout
   numeric=run([str(ROOT/'usr/bin/xprop'),'-id',wid,'-notype','-f','_NET_WM_ICON','32c','_NET_WM_ICON']);(OUT/(lang+'-icon-numeric.txt')).write_text(numeric.stdout)
   row['iconPropertyPresent']='not found' not in numeric.stdout and '=' in numeric.stdout;row['icons']=[]
   if row['iconPropertyPresent']:
    values=list(map(int,re.findall(r'\d+',numeric.stdout.split('=',1)[1])));offset=0
    while offset+2<len(values):
     width,height=values[offset:offset+2];pixels=values[offset+2:offset+2+width*height];assert len(pixels)==width*height
     image=Image.new('RGBA',(width,height));image.putdata([((v>>16)&255,(v>>8)&255,v&255,(v>>24)&255) for v in pixels]);name=f'{lang}-advertised-{width}x{height}.png';image.save(OUT/name)
     row['icons'].append({'width':width,'height':height,'file':name,'matches':{key: image.size==Image.open(ref).size and image.tobytes()==Image.open(ref).convert('RGBA').tobytes() for key,ref in refs.items()}});offset+=2+width*height
   run([str(ROOT/'usr/bin/xdotool'),'windowactivate','--sync',wid],timeout=15);time.sleep(.5);screenshot(lang+'-desktop')
   try:
    run([str(ROOT/'usr/bin/xdotool'),'keydown','Alt_L'],timeout=5);run([str(ROOT/'usr/bin/xdotool'),'key','Tab'],timeout=5);time.sleep(1);screenshot(lang+'-alt-tab')
   finally:run([str(ROOT/'usr/bin/xdotool'),'keyup','Alt_L'],timeout=5)
   row['quit']=m.call('Marionette:Quit',{'flags':['eAttemptQuit']});m.socket.close();m=None;row['exitCode']=p.wait(timeout=30);row['completed']=row['exitCode']==0
  except Exception as e:row['error']=str(e);row['traceback']=traceback.format_exc()
  finally:
   if m:m.socket.close()
   if p.poll() is None:p.terminate();p.wait(timeout=15);row['timedTermination']=True
   log.close();rows.append(row);(OUT/'result.json').write_text(json.dumps(rows,indent=2,ensure_ascii=False));print(json.dumps(row,ensure_ascii=False),flush=True)
  if not row['completed']:break
except Exception as e:
 (OUT/'session-error.json').write_text(json.dumps({'error':str(e),'traceback':traceback.format_exc()},indent=2));print('SESSION_ERROR',str(e),flush=True)
finally:
 for p in reversed(owned):
  if p.poll() is None:
   p.terminate()
   try:p.wait(timeout=12)
   except subprocess.TimeoutExpired:p.kill();p.wait()
 for f in logs:f.close()
 server.shutdown()
