import os,sys,time,subprocess,socket,signal
from pathlib import Path
R=Path(__file__).parent;B=Path('/workspace/qa-linux-22/browser-ui');port=int(sys.argv[1]);product=sys.argv[2];display=':101';profile=R/'runtime'/f'{product}-{port}';profile.mkdir()
(profile/'user.js').write_text(f'user_pref("marionette.port",{port});\nuser_pref("browser.shell.checkDefaultBrowser",false);\nuser_pref("browser.aboutwelcome.enabled",false);\nuser_pref("focusmanager.testmode",false);\n')
deps=Path('/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root');env=os.environ.copy();env.update(XDG_CURRENT_DESKTOP='openbox',DISPLAY=display,MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',PYTHONPATH=str(B/'python'),XDG_CONFIG_HOME=str(R/'runtime/config'),XDG_CACHE_HOME=str(R/'runtime/cache'),XDG_DATA_DIRS=str(deps/'usr/share')+':/usr/local/share:/usr/share',LD_LIBRARY_PATH=str(deps/'usr/lib/x86_64-linux-gnu')+':/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu');assert 'MOZ_DISABLE_CONTENT_SANDBOX' not in env
owned=[]
def launch(args,name):
 f=open(R/'logs'/f'{port}-{name}.log','w');p=subprocess.Popen(args,env=env,stdout=f,stderr=f,start_new_session=True);owned.append(p);return p
try:
 assert not Path('/tmp/.X11-unix/X101').exists(),'display101 already owned'
 x=launch(['/workspace/.floorp-cloud/sysroot/usr/bin/Xvfb',display,'-screen','0','1600x1100x24','-nolisten','tcp'],'xvfb');time.sleep(1);assert x.poll() is None
 w=launch([str(deps/'usr/bin/openbox'),'--sm-disable'],'wm');time.sleep(1);assert w.poll() is None
 p=launch(['dbus-run-session','--',str(B/'products'/product/product),'--no-remote','--profile',str(profile),'--marionette','about:blank'],'browser')
 for _ in range(200):
  assert p.poll() is None
  try:s=socket.create_connection(('localhost',port),.2);s.close();break
  except OSError:time.sleep(.2)
 time.sleep(4)
 subprocess.run([sys.executable,str(R/'cascade-probe.py'),str(port),product,*sys.argv[3:]],env=env,check=True,timeout=300)
finally:
 for p in reversed(owned):
  try:os.killpg(p.pid,signal.SIGTERM);p.wait(timeout=10)
  except ProcessLookupError:pass
  except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL)
