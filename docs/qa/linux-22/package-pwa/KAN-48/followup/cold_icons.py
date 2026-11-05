#!/usr/bin/env python3
"""Unmodified official product, real generated launchers, synthetic controller."""
import os,sys,json,shlex,subprocess,http.server,threading,time,hashlib
from pathlib import Path
B=Path(__file__).resolve().parents[2];D=Path(__file__).resolve().parent
sys.path.insert(0,str(B));from pwa_ui_probe import Fixture,Marionette
sys.path.insert(0,str(B/'python'));from PIL import Image,ImageGrab
e=dict(os.environ,**json.loads((D/'session.json').read_text())['env']);e.update(MOZ_ENABLE_WAYLAND='1',GDK_BACKEND='wayland',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',WAYLAND_DEBUG='client');e.pop('MOZ_DISABLE_CONTENT_SANDBOX',None)
server=http.server.ThreadingHTTPServer(('127.0.0.1',8785),Fixture);threading.Thread(target=server.serve_forever,daemon=True).start()
rows=[]
for entry in json.loads((D/'desktop-entries.json').read_text()):
 pixel=Image.open(entry['icon']).convert('RGBA').getpixel((64,64));label='alpha' if pixel[0]>pixel[2] else 'beta'
 log=(D/f'{label}-cold-wayland.log').open('w');p=subprocess.Popen(shlex.split(entry['exec'])+['--marionette'],env=e,stdout=log,stderr=subprocess.STDOUT);m=None
 row={'label':label,'launcher':entry,'iconExpectedPixel':pixel,'completed':False}
 try:
  for _ in range(120):
   try:m=Marionette('127.0.0.1',2885);break
   except (ConnectionError,OSError):time.sleep(.25)
  if m is None:raise RuntimeError('Marionette unavailable')
  actual=m.script('return Services.dirsvc.get("ProfD",Ci.nsIFile).path');assert actual==str(D/'profile'),actual
  time.sleep(4)
  row['product']=m.script('return {profile:Services.dirsvc.get("ProfD",Ci.nsIFile).path,pid:Services.appinfo.processID,sandbox:Services.prefs.getIntPref("security.sandbox.content.level"),disableEnv:Services.env.get("MOZ_DISABLE_CONTENT_SANDBOX"),experiment:ChromeUtils.importESModule("resource://noraneko/modules/pwa/TaskbarExperiment.sys.mjs").TaskbarExperiment.isEnabledForLinux(),windows:Array.from(Services.wm.getEnumerator("navigator:browser"),w=>({title:w.document.title,url:w.gBrowser?.selectedBrowser?.currentURI?.spec}))}')
  subprocess.run(['sh',str(D/'observe-kde.sh')],env=e,check=True,stdout=subprocess.DEVNULL,timeout=20)
  lines=[s.split('QA_KAN48_WINDOWS=',1)[1] for s in (D/'kwin.log').read_text(errors='replace').splitlines() if 'QA_KAN48_WINDOWS=' in s];row['kwinWindows']=[w for w in json.loads(lines[-1]) if w['pid']==row['product']['pid']]
  shot=ImageGrab.grab(xdisplay=':91');shot.save(D/f'{label}-cold-taskbar.png')
  # Exact opaque fixture colors are measured in the rendered taskbar only.
  bar=shot.crop((0,700,1100,760)).convert('RGBA');points=[(x,y+700) for y in range(bar.height) for x in range(bar.width) if bar.getpixel((x,y))==pixel]
  row['taskbarExactExpectedColorPixels']=len(points);row['taskbarExpectedColorBBox']=[min(x for x,y in points),min(y for x,y in points),max(x for x,y in points),max(y for x,y in points)] if points else None
  row['quit']=m.call('Marionette:Quit',{'flags':['eAttemptQuit']});row['exitCode']=p.wait(timeout=30);row['completed']=True
 except Exception as ex:row['error']=str(ex)
 finally:
  if m:m.socket.close()
  if p.poll() is None:p.terminate();p.wait(timeout=10);row['cleanupTermination']=True
  log.close();rows.append(row);(D/'cold-icon-results.json').write_text(json.dumps(rows,indent=2)+'\n');print(json.dumps(row),flush=True)
server.shutdown()
