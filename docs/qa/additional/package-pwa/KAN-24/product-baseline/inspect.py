import sys,os,json,time,subprocess
from pathlib import Path
sys.path.insert(0,'/workspace/qa-linux-22/package-pwa')
from pwa_ui_probe import Marionette
root=Path(__file__).parent
home=root/'home';profile=root/'profile';home.mkdir(exist_ok=True);profile.mkdir(exist_ok=True)
(profile/'user.js').write_text('user_pref("marionette.port", 2910);\n')
sysroot='/workspace/qa-linux-22/package-pwa/sysroot'
env=dict(os.environ,HOME=str(home),XDG_DATA_HOME=str(home/'.local/share'),XDG_CONFIG_HOME=str(home/'.config'),XDG_CACHE_HOME=str(home/'.cache'),XDG_RUNTIME_DIR='/tmp/kan24-runtime',DISPLAY=':99',LD_LIBRARY_PATH=sysroot+'/usr/lib/x86_64-linux-gnu',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',MOZ_ENABLE_WAYLAND='0',GDK_BACKEND='x11')
for k in list(env):
 if k.startswith('MOZ_DISABLE_'): del env[k]
Path('/tmp/kan24-runtime').mkdir(mode=0o700,exist_ok=True)
x=subprocess.Popen([sysroot+'/usr/bin/Xvfb',':99','-screen','0','1280x900x24','-nolisten','tcp'],env=env,stdout=open(root/'xvfb.log','w'),stderr=subprocess.STDOUT)
time.sleep(1)
p=subprocess.Popen(['dbus-run-session','--','/workspace/qa-linux-full-build/products/3040-r2/floorp/floorp','--no-remote','--profile',str(profile),'--marionette','about:hub'],env=env,cwd='/workspace',stdout=open(root/'browser.log','w'),stderr=subprocess.STDOUT)
(root/'processes.json').write_text(json.dumps({'xvfb':x.pid,'browserLauncher':p.pid}))
for i in range(90):
 try:
  m=Marionette('127.0.0.1',2910);break
 except Exception:time.sleep(1)
else:raise RuntimeError('browser unavailable')
time.sleep(8)
m.call('Marionette:SetContext',{'value':'content'})
r=m.script('return {url:location.href,text:document.body.innerText.slice(0,15000),links:Array.from(document.querySelectorAll("a"),e=>({text:e.textContent,href:e.getAttribute("href")}))}')
(root/'hub-inspect.json').write_text(json.dumps(r,indent=2))
m.call('WebDriver:DeleteSession');m.socket.close()
while True:
 cmd=root/'command.json'
 if cmd.exists():
  request=json.loads(cmd.read_text());cmd.unlink()
  try:
   m=Marionette('127.0.0.1',2910)
   m.call('Marionette:SetContext',{'value':request.get('context','content')})
   if request.get('click'):m.click(request['click']);time.sleep(2)
   value=m.script(request.get('script','return null;'))
   (root/request['out']).write_text(json.dumps(value,indent=2))
   if request.get('screenshot'):
    (root/request['screenshot']).write_bytes(__import__('base64').b64decode(m.call('WebDriver:TakeScreenshot',{'full':False})['value']))
   if request.get('quit'):
    m.call('Marionette:Quit',{'flags':['eAttemptQuit']});p.wait(20);x.terminate();break
   m.call('WebDriver:DeleteSession');m.socket.close()
  except Exception as e:
   (root/request['out']).write_text(json.dumps({'error':str(e)}))
   try:m.call('WebDriver:DeleteSession');m.socket.close()
   except Exception:pass
 time.sleep(.2)
