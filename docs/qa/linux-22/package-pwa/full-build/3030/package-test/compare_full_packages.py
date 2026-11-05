#!/usr/bin/env python3
"""Compare untouched workflow-generated Linux packages with signed language packs.
No manifest/archive modification or product-file replacement is performed.
Run inside dbus-run-session. Each package/locale uses a new dedicated profile.
"""
from pathlib import Path
import argparse,os,sys,json,subprocess,time,socket,hashlib,threading,http.server,re,base64,traceback,zipfile,configparser
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--baseline',type=Path,required=True);p.add_argument('--candidate',type=Path,required=True);p.add_argument('--provenance',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--display',default=':94');p.add_argument('--port',type=int,default=2893);a=p.parse_args()
displayNumber=a.display.lstrip(':').split('.')[0];assert displayNumber.isdigit();assert not Path('/tmp/.X11-unix/X'+displayNumber).exists(),'requested display is already in use';assert not Path('/tmp/.X'+displayNumber+'-lock').exists(),'requested display lock already exists'
try:
 occupied=socket.create_connection(('127.0.0.1',a.port),timeout=.3)
except OSError:pass
else:
 occupied.close();raise RuntimeError('requested Marionette port is already in use')
BASE=Path(__file__).resolve().parents[3];ROOT=BASE/'sysroot';OUT=a.out.resolve();OUT.mkdir(parents=True,exist_ok=False)
sys.path.insert(0,str(BASE));sys.path.insert(0,str(BASE/'python'));from pwa_ui_probe import Marionette
from PIL import Image,ImageGrab
EXPECTED='e55152c559a141001a375ac77098d141b0db3ca7';provenance=json.loads(a.provenance.read_text());assert provenance['candidate']['sourceSha']==EXPECTED;assert provenance['baseline']['sourceSha']=='984509d2053be7bae9027133bda7259cd706b208'
for kind in ['baseline','candidate']:assert provenance[kind]['packageKind']=='workflow-generated-linux-package',kind
packages={'baseline':a.baseline.resolve(strict=True),'candidate':a.candidate.resolve(strict=True)}
assert packages['baseline']!=packages['candidate'];assert all((v/'floorp').is_file() for v in packages.values())
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def inventory(product):return {f.relative_to(product).as_posix():digest(f) for f in sorted(product.rglob('*')) if f.is_file()}
runtimeDir=BASE/'full-build/3030/artifacts';nativeMetadata=json.loads((runtimeDir/'runtime-source-artifact-metadata.json').read_text());assert 'sha256:'+digest(runtimeDir/'runtime-source-provenance.zip')==nativeMetadata['digest'];nativeSource=json.loads((runtimeDir/'runtime-source-provenance.json').read_text());dispatch=json.loads((BASE/'full-build/package-3030-dispatch-payload.json').read_text())['inputs'];assert nativeSource['runtime_commit']==dispatch['runtime_head_sha'];runtimeContract={'version':nativeSource['upstream']['version'],'buildID':dispatch['expected_build_id'],'sourceCommit':nativeSource['runtime_commit'],'sourceArtifactId':nativeMetadata['id'],'sourceArtifactDigest':nativeMetadata['digest'],'inputNativeArtifactId':dispatch['runtime_artifact_id'],'inputNativeArtifactDigest':dispatch['runtime_artifact_digest']};(OUT/'authenticated-runtime-contract.json').write_text(json.dumps(runtimeContract,indent=2))
packagedManifests={};expectedApplication={}
for kind,product in packages.items():
 application=configparser.ConfigParser(interpolation=None);application.read(product/'application.ini');expectedApplication[kind]={'version':application['App']['Version'],'buildID':application['App']['BuildID'],'iniSha256':digest(product/'application.ini'),'declaredUnusedByBrowser':'This file is not used' in (product/'application.ini').read_text()}
 registrations=[]
 for archivePath in [product/'omni.ja',product/'browser/omni.ja']:
  with zipfile.ZipFile(archivePath) as archive:
   for member in archive.namelist():
    if member.endswith('.manifest'):
     for line in archive.read(member).decode().splitlines():
      if 'override chrome://branding/locale/brand.properties' in line:registrations.append({'archive':archivePath.relative_to(product).as_posix(),'member':member,'registration':line})
 packagedManifests[kind]=registrations
expectedOverride='override chrome://branding/locale/brand.properties resource:///chrome/en-US/locale/branding/brand.properties'
assert packagedManifests['baseline']==[]
assert len(packagedManifests['candidate'])==1 and packagedManifests['candidate'][0]['registration']==expectedOverride
(OUT/'packaged-branding-manifests.json').write_text(json.dumps(packagedManifests,indent=2))
subprocess.run(['cc','-O2',str(BASE/'KAN-89/followup/gtk_icon_probe.c'),'-ldl','-o',str(OUT/'gtk-icon-probe')],check=True,capture_output=True,text=True)
before={k:inventory(v) for k,v in packages.items()};(OUT/'package-hashes-before.json').write_text(json.dumps(before,indent=2));(OUT/'provenance.json').write_text(json.dumps(provenance,indent=2))
bundle=(BASE/'branding/colocated-bundle.js').read_text();assert hashlib.sha256(bundle.encode()).hexdigest()=='fe26001bfd208cff9660b3e242f96c757b3c3fa5abf89fa587e86a835690dfd1'
refs={name:Image.open(BASE/('branding/'+file)).convert('RGBA') for name,file in [('firefox','firefox-official64.png')]};refs['floorp']=Image.open(BASE/'product/floorp/browser/chrome/icons/default/default64.png').convert('RGBA')
for name,image in refs.items():image.save(OUT/(name+'-reference64.png'))
runtime=OUT/'runtime';runtime.mkdir();runtime.chmod(0o700);desktopHome=OUT/'desktop-home';desktopHome.mkdir()
env=dict(os.environ,HOME=str(desktopHome),DISPLAY=a.display,XDG_RUNTIME_DIR=str(runtime),XDG_DATA_DIRS=str(ROOT/'usr/share')+':/usr/share',XDG_CONFIG_DIRS=str(ROOT/'etc/xdg'),QT_PLUGIN_PATH=str(ROOT/'usr/lib/x86_64-linux-gnu/qt6/plugins'),QML2_IMPORT_PATH=str(ROOT/'usr/lib/x86_64-linux-gnu/qt6/qml'),LD_LIBRARY_PATH=str(ROOT/'usr/lib/x86_64-linux-gnu'),PATH=str(ROOT/'usr/bin')+':'+os.environ.get('PATH',''),QT_QPA_PLATFORM='xcb',LIBGL_DRIVERS_PATH=str(ROOT/'usr/lib/x86_64-linux-gnu/dri'),__EGL_VENDOR_LIBRARY_FILENAMES=str(ROOT/'usr/share/glvnd/egl_vendor.d/50_mesa.json'))
for key in ['MOZ_DISABLE_CONTENT_SANDBOX','WAYLAND_DISPLAY']:env.pop(key,None)
class Page(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  title='Fullpackage '+self.path.strip('/');body=f'<meta charset="utf-8"><title>{title}</title><p>日本語 Español English</p>'.encode();self.send_response(200);self.send_header('Content-Type','text/html;charset=utf-8');self.end_headers();self.wfile.write(body)
 def log_message(self,*args):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',8893),Page);threading.Thread(target=server.serve_forever,daemon=True).start();owned=[];logs=[];rows=[]
def run(cmd,**kwargs):return subprocess.run(cmd,env=env,capture_output=True,text=True,**kwargs)
def start(name,cmd):
 log=(OUT/(name+'.log')).open('w');logs.append(log);proc=subprocess.Popen(cmd,env=env,stdout=log,stderr=subprocess.STDOUT);owned.append(proc);return proc

def launch(product,label,locale,home,profile):
 user=profile/'user.js';user.write_text('user_pref("marionette.port",'+str(a.port)+');\nuser_pref("intl.locale.requested",'+json.dumps(locale)+');\n')
 e=dict(env,HOME=str(home),XDG_CONFIG_HOME=str(home/'.config'),XDG_DATA_HOME=str(home/'.local/share'),XDG_CACHE_HOME=str(home/'.cache'),MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',MOZ_MARIONETTE='1',MOZ_MARIONETTE_PREF_STATE_ACROSS_RESTARTS=json.dumps({'marionette.port':a.port}),GDK_BACKEND='x11',MOZ_ENABLE_WAYLAND='0')
 log=(OUT/(label+'-browser.log')).open('w');logs.append(log);proc=subprocess.Popen([str(product/'floorp'),'--no-remote','--profile',str(profile),'--marionette','http://127.0.0.1:8893/'+label],env=e,stdout=log,stderr=subprocess.STDOUT);owned.append(proc)
 client=None
 for _ in range(180):
  try:client=Marionette('127.0.0.1',a.port);break
  except (ConnectionError,OSError):time.sleep(.25)
 if client is None:raise RuntimeError(label+': Marionette unavailable')
 actual=client.script("return Services.dirsvc.get('ProfD',Ci.nsIFile).path;");assert actual==str(profile),actual
 return proc,client

def quit_owned(proc,client):
 q=client.call('Marionette:Quit',{'flags':['eAttemptQuit']});client.socket.close();code=proc.wait(timeout=35);assert code==0,code;return {'request':q,'exitCode':code}
try:
 x=start('xvfb',[str(ROOT/'usr/bin/Xvfb'),a.display,'-screen','0','1280x900x24','-nolisten','tcp'])
 for _ in range(50):
  if run([str(ROOT/'usr/bin/xdotool'),'getdisplaygeometry']).returncode==0:break
  if x.poll() is not None:raise RuntimeError('Xvfb startup failed')
  time.sleep(.2)
 wm=start('kwin-x11',[str(ROOT/'usr/bin/kwin_x11'),'--replace']);time.sleep(3)
 support=run([str(ROOT/'usr/bin/xprop'),'-root','_NET_SUPPORTING_WM_CHECK','_NET_SUPPORTED']);(OUT/'wm-properties.txt').write_text(support.stdout);assert '_NET_SUPPORTING_WM_CHECK(WINDOW)' in support.stdout
 for kind,product in packages.items():
  for locale in ['ja','es-CL']:
   label=kind+'-'+locale;home=OUT/(label+'-home');profile=home/'profile';profile.mkdir(parents=True)
   fontdir=home/'.local/share/fonts';fontdir.mkdir(parents=True);import shutil;shutil.copyfile(ROOT/'usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc',fontdir/'NotoSansCJK-Regular.ttc')
   theme=home/'.icons/qa-fullpackage';(theme/'64x64/apps').mkdir(parents=True);(theme/'index.theme').write_text('[Icon Theme]\nName=QA Fullpackage\nDirectories=64x64/apps\nInherits=hicolor\n\n[64x64/apps]\nSize=64\nType=Fixed\nContext=Applications\n')
   for name,image in refs.items():image.save(theme/('64x64/apps/'+name+'.png'))
   settings=home/'.config/gtk-3.0';settings.mkdir(parents=True);(settings/'settings.ini').write_text('[Settings]\ngtk-icon-theme-name=qa-fullpackage\n')
   themeEnv=dict(env,HOME=str(home),XDG_CONFIG_HOME=str(home/'.config'),XDG_DATA_HOME=str(home/'.local/share'),XDG_CACHE_HOME=str(home/'.cache'),GDK_BACKEND='x11',MOZ_ENABLE_WAYLAND='0');themeProbe=subprocess.run([str(OUT/'gtk-icon-probe')],env=themeEnv,capture_output=True,text=True,timeout=20);(OUT/(label+'-gtk-theme.txt')).write_text(themeProbe.stdout+themeProbe.stderr);assert themeProbe.returncode==0 and 'theme=qa-fullpackage' in themeProbe.stdout and 'icon=floorp sizes=64,' in themeProbe.stdout and 'icon=firefox sizes=64,' in themeProbe.stdout
   row={'themeVariant':'deliberate controlled qa-fullpackage fixture with both known64px PNGs; not default desktop theme','themeAssetSha256':{name:digest(theme/('64x64/apps/'+name+'.png')) for name in refs},'packagedBrandingOverride':packagedManifests[kind],'packagedRootApplicationMetadata':expectedApplication[kind],'expectedNativeRuntimeApplication':runtimeContract,'kind':kind,'locale':locale,'sourceSha':provenance[kind]['sourceSha'],'package':str(product),'profile':str(profile),'completed':False,'input':'synthetic Marionette UI click; no physical-input claim','packageFilesModified':False}
   client=None
   try:
    proc,client=launch(product,label+'-install','en-US',home,profile)
    row['install']=client.async_script("const done=arguments[arguments.length-1];ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs').AddonManager.getInstallForURL(arguments[0]).then(i=>{i.addListener({onInstallEnded(_i,a){done({id:a.id,signedState:a.signedState,active:a.isActive,version:a.version});},onDownloadFailed(i){done({error:i.error});},onInstallFailed(i){done({error:i.error});}});i.install();}).catch(e=>done({error:String(e)}));",['file://'+str(BASE/('branding/'+locale+'.xpi'))]);assert row['install']['signedState']==2,row['install']
    row['installQuit']=quit_owned(proc,client);client=None
    proc,client=launch(product,label,locale,home,profile);time.sleep(3)
    row['product']=client.script("const w=Services.wm.getMostRecentWindow('navigator:browser');return {profile:Services.dirsvc.get('ProfD',Ci.nsIFile).path,executable:Services.dirsvc.get('XREExeF',Ci.nsIFile).path,version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,platformVersion:Services.appinfo.platformVersion,platformBuildID:Services.appinfo.platformBuildID,locale:Services.locale.appLocaleAsBCP47,brand:Services.strings.createBundle('chrome://branding/locale/brand.properties').GetStringFromName('brandShortName'),sandbox:Services.prefs.getIntPref('security.sandbox.content.level'),sandboxDisableEnv:Services.env.exists('MOZ_DISABLE_CONTENT_SANDBOX'),title:w.document.title,geometry:{x:w.screenX,y:w.screenY,width:w.outerWidth,height:w.outerHeight}}; ")
    row['requestedLocales']=client.script('return Services.locale.requestedLocales;');row['activeLanguagePack']=client.async_script("const done=arguments[arguments.length-1];ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs').AddonManager.getAddonByID(arguments[0]).then(a=>done(a?{id:a.id,active:a.isActive,signedState:a.signedState,version:a.version}:null));",['langpack-'+locale+'@firefox.mozilla.org']);assert row['requestedLocales'][0]==locale;assert row['activeLanguagePack'] and row['activeLanguagePack']['active'] and row['activeLanguagePack']['signedState']==2;assert row['product']['locale']==locale;assert row['product']['version']==runtimeContract['version'] and row['product']['buildID']==runtimeContract['buildID'];assert row['product']['sandbox']==6 and not row['product']['sandboxDisableEnv'];assert Path(row['product']['executable']).resolve()==(product/'floorp').resolve()
    row['regression']=client.async_script('const done=arguments[arguments.length-1];'+bundle+'\nBrandingRegression.runAllTests().then(()=>done({pass:true,progress:globalThis.__NORA_TEST_PROGRESS__})).catch(e=>done({pass:false,error:String(e),progress:globalThis.__NORA_TEST_PROGRESS__}));')
    clients=run([str(ROOT/'usr/bin/xprop'),'-root','_NET_CLIENT_LIST']);(OUT/(label+'-client-list.txt')).write_text(clients.stdout);matches=[]
    for wid in re.findall(r'0x[0-9a-f]+',clients.stdout):
     props=run([str(ROOT/'usr/bin/xprop'),'-id',wid,'WM_STATE','WM_CLASS','_NET_WM_NAME','_NET_WM_PID','_NET_WM_ICON']);(OUT/(label+'-'+wid+'-xprop.txt')).write_text(props.stdout)
     if 'Fullpackage '+label+' ' in props.stdout or 'Fullpackage '+label+'—' in props.stdout or 'Fullpackage '+label+'\\"' in props.stdout:matches.append(wid)
     elif 'Fullpackage '+label in props.stdout and label+'-install' not in props.stdout:matches.append(wid)
    assert len(matches)==1,matches;wid=matches[0];row['windowID']=wid
    numeric=run([str(ROOT/'usr/bin/xprop'),'-id',wid,'-notype','-f','_NET_WM_ICON','32c','_NET_WM_ICON']);(OUT/(label+'-icon-numeric.txt')).write_text(numeric.stdout);row['iconPresent']='=' in numeric.stdout and 'not found' not in numeric.stdout;row['icons']=[]
    if row['iconPresent']:
     values=list(map(int,re.findall(r'\d+',numeric.stdout.split('=',1)[1])));offset=0
     while offset+2<len(values):
      width,height=values[offset:offset+2];pixels=values[offset+2:offset+2+width*height];assert len(pixels)==width*height;image=Image.new('RGBA',(width,height));image.putdata([((v>>16)&255,(v>>8)&255,v&255,(v>>24)&255) for v in pixels]);image.save(OUT/(label+f'-icon-{width}x{height}.png'));row['icons'].append({'width':width,'height':height,'matches':{name:image.size==ref.size and image.tobytes()==ref.tobytes() for name,ref in refs.items()}});offset+=2+width*height
    client.click('#PanelUI-menu-button');time.sleep(.8);row['menu']=client.script("const row=document.getElementById('appMenu-settings-button');const rect=row.getBoundingClientRect();return {state:document.getElementById('appMenu-popup')?.state,settingsVisibility:getComputedStyle(row).visibility,settingsRect:{width:rect.width,height:rect.height},labels:['appMenu-settings-button','appMenu-new-window-button2'].map(id=>({id,label:document.getElementById(id)?.label}))};");ImageGrab.grab(xdisplay=a.display).save(OUT/(label+'-menu-root.png'))
    row['quit']=quit_owned(proc,client);client=None;row['completed']=True
    row['expectedBrand']='Floorp' if kind=='candidate' else 'Firefox';row['brandingMatchesExpected']=row['product']['brand']==row['expectedBrand'];row['regressionMatchesExpected']=row['regression']['pass']==(kind=='candidate');expectedIcon='floorp' if kind=='candidate' else 'firefox';row['iconMatchesExpected']=any(i['matches'].get(expectedIcon) for i in row['icons']);expectedLabels=['設定','新しいウィンドウ'] if locale=='ja' else ['Ajustes','Nueva ventana'];row['menuMatchesLocale']=row['menu']['state']=='open' and row['menu']['settingsVisibility']=='visible' and row['menu']['settingsRect']['width']>0 and [i['label'] for i in row['menu']['labels']]==expectedLabels;row['comparisonPass']=row['brandingMatchesExpected'] and row['regressionMatchesExpected'] and row['iconMatchesExpected'] and row['menuMatchesLocale']
   except Exception as error:row['error']=str(error);row['traceback']=traceback.format_exc()
   finally:
    if client:client.socket.close()
    if 'proc' in locals() and proc.poll() is None:proc.terminate();proc.wait(timeout=15);row['timedTermination']=True
    rows.append(row);(OUT/'results.json').write_text(json.dumps(rows,indent=2,ensure_ascii=False));print(json.dumps(row,ensure_ascii=False),flush=True)
   if not row['completed']:raise RuntimeError(label+' did not complete')
except Exception as error:(OUT/'controller-error.json').write_text(json.dumps({'error':str(error),'traceback':traceback.format_exc()},indent=2))
finally:
 for proc in reversed(owned):
  if proc.poll() is None:
   proc.terminate()
   try:proc.wait(timeout=12)
   except subprocess.TimeoutExpired:proc.kill();proc.wait()
 for log in logs:log.close()
 server.shutdown();after={k:inventory(v) for k,v in packages.items()};(OUT/'package-hashes-after.json').write_text(json.dumps(after,indent=2));(OUT/'summary.json').write_text(json.dumps({'rowsCompleted':sum(r['completed'] for r in rows),'rowsExpected':4,'allComparisonsPass':len(rows)==4 and all(r.get('comparisonPass',False) for r in rows),'allPackageFilesUnchanged':before==after,'candidateSourceSha':EXPECTED,'nativeBuildOrSigningClaim':'workflow-produced artifact-mode package; no claim of native recompilation/signing beyond workflow provenance','manualProductPatch':False},indent=2))
