#!/usr/bin/env python3
"""Cold launch actual generated original-site PWA, preserve product & launcher."""
from pathlib import Path
import os,sys,json,subprocess,time,shlex,hashlib,base64,traceback
D=Path(__file__).resolve().parent;B=Path('/workspace/qa-linux-22/package-pwa');sys.path.insert(0,str(B));from pwa_ui_probe import Marionette,SNAPSHOT
sys.path.insert(0,str(B/'python'));from PIL import ImageGrab,Image
s=json.loads((D/'session.json').read_text());e=dict(os.environ,**s['env']);e.update(MOZ_ENABLE_WAYLAND='1',GDK_BACKEND='wayland',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',WAYLAND_DEBUG='client')
for k in list(e):
 if k.startswith('MOZ_DISABLE_'):del e[k]
P=Path('/workspace/qa-linux-additional/KAN-24/followup/products/baseline/floorp')
def inventory():return {str(p.relative_to(P)):hashlib.sha256(p.read_bytes()).hexdigest() for p in P.rglob('*') if p.is_file()}
before=inventory();desktop=next((D/'home/.local/share/applications').glob('floorp-*.desktop'));text=desktop.read_text();entry=dict(l.split('=',1) for l in text.splitlines() if '='in l);icon=Path(entry['Icon']);row={'sourceSha':'29a3eb23145da90706d8a94441373278521cc737','actualSite':'https://www.vrt.be/vrtmax/','desktop':{k:entry[k] for k in ['Name','Exec','Icon','StartupWMClass','X-Floorp-Id','X-Floorp-StartUrl']},'desktopFile':desktop.name,'desktopSha256':hashlib.sha256(desktop.read_bytes()).hexdigest(),'iconSha256':hashlib.sha256(icon.read_bytes()).hexdigest(),'iconDimensions':Image.open(icon).size,'productUnchanged':False};m=None;p=None
try:
 p=subprocess.Popen(shlex.split(entry['Exec'])+['--marionette'],env=e,stdout=(D/'vrtmax-cold-wayland.log').open('w'),stderr=subprocess.STDOUT)
 for i in range(160):
  if p.poll() is not None:raise RuntimeError('product exited early')
  try:m=Marionette('127.0.0.1',2926);break
  except OSError:time.sleep(.25)
 assert m
 time.sleep(8);row['product']=m.script(SNAPSHOT);row['native']=m.script('return {pid:Services.appinfo.processID,profile:Services.dirsvc.get("ProfD",Ci.nsIFile).path,sandbox:Services.prefs.getIntPref("security.sandbox.content.level"),disable:Services.env.exists("MOZ_DISABLE_CONTENT_SANDBOX"),experiment:ChromeUtils.importESModule("resource://noraneko/modules/pwa/TaskbarExperiment.sys.mjs").TaskbarExperiment.isEnabledForLinux()};');assert row['native']['sandbox']==6 and not row['native']['disable']
 subprocess.run(['sh',str(D/'observe-kde.sh')],env=e,check=True,stdout=(D/'observe-kde.log').open('w'),stderr=subprocess.STDOUT,timeout=20)
 lines=[l.split('QA_KAN48_WINDOWS=',1)[1] for l in (D/'kwin.log').read_text(errors='replace').splitlines() if 'QA_KAN48_WINDOWS=' in l];row['kwinWindows']=[w for w in json.loads(lines[-1]) if w['pid']==row['native']['pid']]
 image=ImageGrab.grab(xdisplay=':106');image.save(D/'vrtmax-cold-taskbar.png');image.crop((0,700,1100,760)).save(D/'vrtmax-taskbar-region.png')
 row['normalQuit']=m.call('Marionette:Quit',{'flags':['eAttemptQuit']});row['exitCode']=p.wait(40);row['completed']=True
except Exception as ex:row['error']=str(ex);row['traceback']=traceback.format_exc()
finally:
 if m:m.socket.close()
 if p and p.poll() is None:p.terminate();p.wait(15);row['forcedTermination']=True
 row['productUnchanged']=before==inventory();row['launcherUnchanged']=row['desktopSha256']==hashlib.sha256(desktop.read_bytes()).hexdigest();row['ownedBrowserRemaining']=bool(p and p.poll() is None);(D/'cold-result.json').write_text(json.dumps(row,indent=2)+'\n');print(json.dumps(row,indent=2))
