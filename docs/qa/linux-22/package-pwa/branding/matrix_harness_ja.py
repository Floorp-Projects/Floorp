#!/usr/bin/env python3
"""Dedicated-profile locale comparison; no sandbox/experiment changes."""
from pathlib import Path
import os,sys,json,socket,subprocess,time,signal,hashlib,zipfile
b=Path(__file__).resolve().parent;root=b.parent
sys.path.insert(0,str(root));from pwa_ui_probe import Marionette
expected=str(b/'profile-harness')
def stop():
 try:
  m=Marionette('127.0.0.1',2869)
  profile=m.script("return Services.dirsvc.get('ProfD',Ci.nsIFile).path;")
  assert profile==expected,profile
  pid=m.script('return Services.appinfo.processID;');m.call('WebDriver:DeleteSession');m.socket.close();os.kill(pid,signal.SIGTERM)
  for _ in range(100):
   try:os.kill(pid,0)
   except ProcessLookupError:break
   time.sleep(.1)
 except ConnectionRefusedError:pass

env=dict(os.environ,DISPLAY=':88',HOME=str(b/'home'),XDG_RUNTIME_DIR=str(b/'runtime'),MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',LD_LIBRARY_PATH=str(root/'sysroot/usr/lib/x86_64-linux-gnu'))
env.pop('MOZ_DISABLE_CONTENT_SANDBOX',None)
env.update(MOZ_MARIONETTE='1',GDK_BACKEND='x11',MOZ_ENABLE_WAYLAND='0',MOZ_MARIONETTE_PREF_STATE_ACROSS_RESTARTS=json.dumps({'marionette.port':2869}),QA_BRANDING_PORT='2869',QA_BRANDING_PROFILE=expected)
rows=[]
for kind,locale in [('candidate','ja')]:
 stop();label=kind+'-'+locale
 (b/'profile-harness/user.js').write_text('user_pref("marionette.port",2869);\nuser_pref("intl.locale.requested",'+json.dumps(locale)+');\n')
 product=root/'product/floorp/floorp' if kind=='baseline' else b/'candidate-floorp/floorp'
 log=(b/(label+'-matrix-browser.log')).open('wb')
 process=subprocess.Popen(['dbus-run-session','--',str(product),'--no-remote','--profile',expected,'--marionette','about:blank'],env=env,stdout=log,stderr=subprocess.STDOUT)
 for _ in range(150):
  if process.poll() is not None:raise RuntimeError(f'{label} browser exited {process.returncode}')
  try:
   with socket.create_connection(('127.0.0.1',2869),.2):break
  except OSError:time.sleep(.2)
 time.sleep(2)
 for command in [['probe.py',label],['regression_harness.py',label+'-harness']]:
  result=subprocess.run([sys.executable,str(b/command[0]),command[1]],env=env,capture_output=True,text=True,timeout=90)
  (b/(label+'-'+command[0]+'.log')).write_text(result.stdout+result.stderr)
  if result.returncode:raise RuntimeError(f'{label} {command[0]} failed: {result.stderr}')
 rows.append({'kind':kind,'locale':locale,'result':json.loads((b/(label+'-harness-regression.json')).read_text())})
 stop();process.wait(timeout=15);log.close()
(b/'harness-ja-matrix.json').write_text(json.dumps(rows,indent=2,ensure_ascii=False));print(json.dumps([{'kind':r['kind'],'locale':r['locale'],'pass':r['result']['pass'],'menu':r['result'].get('menu')} for r in rows],ensure_ascii=False))
