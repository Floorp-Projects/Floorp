#!/usr/bin/env python3
"""Official Flatpak comparison. Uses synthetic automation, retains sandbox prefs."""
import os,sys,time,json,subprocess,base64,shutil,http.server,threading,traceback
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from pwa_ui_probe import Marionette
B=Path(__file__).resolve().parents[1]; OUT=B/'KAN-17'; ROOT=B/'sysroot'
class Page(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  body='<meta charset="utf-8"><title>Japanese fixture</title><p style="font:32px sans-serif">日本語の文字 漢字 かな カナ</p><p>English control</p>'.encode()
  self.send_response(200);self.send_header('Content-Type','text/html; charset=utf-8');self.end_headers();self.wfile.write(body)
 def log_message(self,*a): pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',8876),Page);threading.Thread(target=server.serve_forever,daemon=True).start()
results=[]
for index,(app,fonts) in enumerate([(a,f) for a in ['one.ablaze.floorp','org.mozilla.firefox'] for f in [False,True]]):
 label=app+('-noto' if fonts else '-default'); home=OUT/label;home.mkdir(exist_ok=True)
 profile=home/'.var/app'/app/'config/qa-profile';profile.mkdir(parents=True,exist_ok=True)
 port=2841+index
 (profile/'user.js').write_text(f'user_pref("marionette.port", {port});\n')
 if fonts:
  dest=home/'.local/share/fonts';dest.mkdir(parents=True,exist_ok=True)
  shutil.copyfile(ROOT/'usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc',dest/'NotoSansCJK-Regular.ttc')
 env=dict(os.environ,HOME=str(home),XDG_CONFIG_HOME=str(home/'.config'),XDG_DATA_HOME=str(B/'flatpak-home/.local/share'),XDG_CACHE_HOME=str(home/'.cache'),XDG_RUNTIME_DIR=str(B/'runtime'),DISPLAY=':88',LD_LIBRARY_PATH=str(ROOT/'usr/lib/x86_64-linux-gnu'),FLATPAK_BWRAP='/usr/bin/bwrap',FLATPAK_DBUSPROXY=str(ROOT/'usr/bin/xdg-dbus-proxy'))
 env.pop('MOZ_DISABLE_CONTENT_SANDBOX',None)
 flatpak=str(ROOT/'usr/bin/flatpak')
 base=[flatpak,'run','--user']
 probe=subprocess.run(base+['--command=sh',app,'-c','printf "JA_FONTS\\n"; fc-list :lang=ja family; printf "MATCH\\n"; fc-match :lang=ja'],env=env,capture_output=True,text=True,timeout=30)
 (OUT/(label+'-fontconfig.log')).write_text(probe.stdout+probe.stderr)
 log=(OUT/(label+'-browser.log')).open('w')
 p=subprocess.Popen(['dbus-run-session','--']+base+['--env=MOZ_REMOTE_ALLOW_SYSTEM_ACCESS=1',app,'--no-remote','--profile',str(profile),'--marionette','http://127.0.0.1:8876/'],env=env,stdout=log,stderr=subprocess.STDOUT)
 row={'app':app,'fontAdded':fonts,'fontconfigExit':probe.returncode,'profile':str(profile),'completed':False}
 browser=None
 try:
  for attempt in range(60):
   try: browser=Marionette('127.0.0.1',port);break
   except (ConnectionError,OSError):time.sleep(.5)
  if browser is None:raise RuntimeError('Marionette unavailable')
  actual=browser.script('return Services.dirsvc.get("ProfD",Ci.nsIFile).path')
  if actual!=str(profile):raise RuntimeError('Profile mismatch')
  row.update(browser.script('return {version:Services.appinfo.version,os:Services.appinfo.OS,sandbox:Services.prefs.getIntPref("security.sandbox.content.level"),locale:Services.locale.appLocaleAsBCP47}'))
  browser.call('Marionette:SetContext',{'value':'content'})
  time.sleep(2)
  row['render']=browser.script('const c=document.createElement("canvas");const x=c.getContext("2d");x.font="32px sans-serif";return {text:document.querySelector("p").textContent,width:x.measureText("日本語の文字 漢字 かな カナ").width,ready:document.fonts.status}')
  shot=browser.call('WebDriver:TakeScreenshot',{'full':False});(OUT/(label+'.png')).write_bytes(base64.b64decode(shot.get('value',shot)))
  row['quit']=browser.call('Marionette:Quit',{'flags':['eAttemptQuit']});row['completed']=True
 except Exception as e:row['error']=str(e);row['traceback']=traceback.format_exc()
 finally:
  if browser:browser.socket.close()
  try:p.wait(timeout=20)
  except subprocess.TimeoutExpired:p.terminate();p.wait(timeout=10);row['timedTermination']=True
  log.close();results.append(row);(OUT/'font-results.json').write_text(json.dumps(results,indent=2,ensure_ascii=False));print(json.dumps(row,ensure_ascii=False),flush=True)
server.shutdown()
