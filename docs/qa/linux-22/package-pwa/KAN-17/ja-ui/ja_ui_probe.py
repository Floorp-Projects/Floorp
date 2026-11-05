#!/usr/bin/env python3
"""Actual official Flatpak Japanese UI and text probe. Synthetic automation only.
No security, signature or experiment policy changes. Uses a new dedicated HOME.
"""
from pathlib import Path
import sys,os,json,subprocess,socket,time,shutil,threading,http.server,base64,traceback,hashlib
OUT=Path(__file__).resolve().parent;B=OUT.parents[1];ROOT=B/'sysroot';HOME=OUT/'new-home';APP=HOME/'.var/app/one.ablaze.floorp';PROFILE=APP/'config/qa-ja-ui-profile';PORT=2875
sys.path.insert(0,str(B));from pwa_ui_probe import Marionette
PROFILE.mkdir(parents=True,exist_ok=True)
fonts=HOME/'.local/share/fonts';fonts.mkdir(parents=True,exist_ok=True)
font=ROOT/'usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc';shutil.copyfile(font,fonts/font.name)
xpi=APP/'config/ja.xpi';shutil.copyfile(B/'branding/ja.xpi',xpi)
class Fixture(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  body='<meta charset="utf-8"><title>JA UI controlled fixture</title><p style="font:32px sans-serif">日本語の文字 漢字 かな カナ</p><p>English control</p>'.encode()
  self.send_response(200);self.send_header('Content-Type','text/html;charset=utf-8');self.end_headers();self.wfile.write(body)
 def log_message(self,*args):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',8879),Fixture);threading.Thread(target=server.serve_forever,daemon=True).start()
env=dict(os.environ,HOME=str(HOME),XDG_DATA_HOME=str(B/'flatpak-home/.local/share'),XDG_CONFIG_HOME=str(HOME/'.config'),XDG_CACHE_HOME=str(HOME/'.cache'),XDG_RUNTIME_DIR=str(B/'runtime'),DISPLAY=':88',LD_LIBRARY_PATH=str(ROOT/'usr/lib/x86_64-linux-gnu'),FLATPAK_BWRAP='/usr/bin/bwrap',FLATPAK_DBUSPROXY=str(ROOT/'usr/bin/xdg-dbus-proxy'),DBUS_SYSTEM_BUS_ADDRESS='unix:path='+str(B/'KAN-8/system-probe-bus'))
env.pop('MOZ_DISABLE_CONTENT_SANDBOX',None)
flatpak=str(ROOT/'usr/bin/flatpak');base=[flatpak,'run','--user'];results=[]
probe=subprocess.run(base+['--command=sh','one.ablaze.floorp','-c','fc-list :lang=ja family; fc-match :lang=ja'],env=env,capture_output=True,text=True,timeout=30)
(OUT/'fontconfig.log').write_text(probe.stdout+probe.stderr)
for locale in ['en-US','ja']:
 (PROFILE/'user.js').write_text('user_pref("marionette.port",'+str(PORT)+');\nuser_pref("intl.locale.requested",'+json.dumps(locale)+');\n')
 cmd=['dbus-run-session','--']+base+['--env=GDK_BACKEND=x11','--env=MOZ_ENABLE_WAYLAND=0','--env=MOZ_REMOTE_ALLOW_SYSTEM_ACCESS=1','--env=MOZ_MARIONETTE_PREF_STATE_ACROSS_RESTARTS='+json.dumps({'marionette.port':PORT}),'one.ablaze.floorp','--no-remote','--profile',str(PROFILE),'--marionette','http://127.0.0.1:8879/']
 row={'requestedLocale':locale,'profile':str(PROFILE),'completed':False,'input':'synthetic Marionette element click; not physical input','fixture':'local controlled Japanese/English text','fontSha256':hashlib.sha256(font.read_bytes()).hexdigest(),'jaXpiSha256':hashlib.sha256(xpi.read_bytes()).hexdigest()}
 log=(OUT/(locale+'-browser.log')).open('w');p=subprocess.Popen(cmd,env=env,stdout=log,stderr=subprocess.STDOUT);m=None
 try:
  for _ in range(120):
   try:m=Marionette('127.0.0.1',PORT);break
   except (ConnectionError,OSError):time.sleep(.3)
  if m is None:raise RuntimeError('Marionette unavailable')
  actual=m.script("return Services.dirsvc.get('ProfD',Ci.nsIFile).path;");assert actual==str(PROFILE),actual
  row.update(m.script("return {actualLocale:Services.locale.appLocaleAsBCP47,executable:Services.dirsvc.get('XREExeF',Ci.nsIFile).path,version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,sandbox:Services.prefs.getIntPref('security.sandbox.content.level'),sandboxDisableEnv:Services.env.exists('MOZ_DISABLE_CONTENT_SANDBOX')};"))
  if locale=='en-US':
   row['install']=m.async_script("const done=arguments[arguments.length-1];const {AddonManager}=ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs');AddonManager.getInstallForURL(arguments[0]).then(i=>{i.addListener({onInstallEnded(_i,a){done({id:a.id,signedState:a.signedState,active:a.isActive});},onDownloadFailed(i){done({error:i.error});},onInstallFailed(i){done({error:i.error});}});i.install();}).catch(e=>done({error:String(e)}));",['file://'+str(xpi)])
   assert row['install'].get('signedState')==2,row['install']
  else:
   row['pack']=m.async_script("const done=arguments[arguments.length-1];ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs').AddonManager.getAddonByID('langpack-ja@firefox.mozilla.org').then(a=>done({id:a.id,signedState:a.signedState,active:a.isActive,version:a.version}));")
   assert row['actualLocale']=='ja',row
   assert row['pack']['active'] and row['pack']['signedState']==2,row
   m.call('Marionette:SetContext',{'value':'content'});time.sleep(2)
   row['render']=m.script('const c=document.createElement("canvas");const x=c.getContext("2d");x.font="32px sans-serif";return {text:document.querySelector("p").textContent,width:x.measureText("日本語の文字 漢字 かな カナ").width,fontStatus:document.fonts.status}')
   shot=m.call('WebDriver:TakeScreenshot',{'full':False});(OUT/'ja-fixture.png').write_bytes(base64.b64decode(shot.get('value',shot)))
   m.call('Marionette:SetContext',{'value':'chrome'});m.click('#PanelUI-menu-button');time.sleep(.8)
   row['menu']=m.script("return ['appMenu-settings-button','appMenu-new-window-button2'].map(id=>({id,label:document.getElementById(id)?.label}));")
   assert row['menu'][0]['label']=='設定' and row['menu'][1]['label']=='新しいウィンドウ',row['menu']
   shot=m.call('WebDriver:TakeScreenshot',{'full':True});(OUT/'ja-menu.png').write_bytes(base64.b64decode(shot.get('value',shot)))
  row['quit']=m.call('Marionette:Quit',{'flags':['eAttemptQuit']});m.socket.close();m=None;row['exitCode']=p.wait(timeout=30);row['completed']=row['exitCode']==0
 except Exception as e:row['error']=str(e);row['traceback']=traceback.format_exc()
 finally:
  if m:m.socket.close()
  if p.poll() is None:p.terminate();p.wait(timeout=15);row['timedTermination']=True
  log.close();results.append(row);(OUT/'result.json').write_text(json.dumps(results,indent=2,ensure_ascii=False));print(json.dumps(row,ensure_ascii=False),flush=True)
 if not row['completed']:break
server.shutdown()
