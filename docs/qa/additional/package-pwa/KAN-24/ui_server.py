import sys,os,json,time,subprocess,base64,traceback
from pathlib import Path
sys.path.insert(0,'/workspace/qa-linux-22/package-pwa');from pwa_ui_probe import Marionette
sys.path.insert(0,'/workspace/qa-linux-22/package-pwa/full-build');from full_package_pwa_probe import inventory
root=Path(sys.argv[1]);product=Path(sys.argv[2]);port=int(sys.argv[3]);root.mkdir(parents=True,exist_ok=True)
home=root/'home';profile=root/'profile';home.mkdir(exist_ok=True);profile.mkdir(exist_ok=True)
(profile/'user.js').write_text(f'user_pref("marionette.port",{port});\n')
(root/'inventory-before.json').write_text(json.dumps(inventory(product)))
s='/workspace/qa-linux-22/package-pwa/sysroot';env=dict(os.environ,HOME=str(home),XDG_CONFIG_HOME=str(home/'.config'),XDG_DATA_HOME=str(home/'.local/share'),XDG_CACHE_HOME=str(home/'.cache'),XDG_RUNTIME_DIR='/tmp/kan24-runtime',DISPLAY=':100',LD_LIBRARY_PATH=s+'/usr/lib/x86_64-linux-gnu',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',MOZ_ENABLE_WAYLAND='0',GDK_BACKEND='x11')
for k in list(env):
 if k.startswith('MOZ_DISABLE_'):del env[k]
display_process=subprocess.Popen([s+'/usr/bin/Xvfb',':100','-screen','0','1280x900x24','-nolisten','tcp'],env=env,stdout=open(root/'xvfb.log','w'),stderr=subprocess.STDOUT)
for i in range(50):
 if display_process.poll() is not None:raise RuntimeError('owned Xvfb failed')
 if Path('/tmp/.X11-unix/X100').exists():break
 time.sleep(.1)
p=subprocess.Popen(['dbus-run-session','--',str(product/'floorp'),'--no-remote','--profile',str(profile),'--marionette',(sys.argv[4] if len(sys.argv)>4 else 'about:hub#/features/design')],env=env,cwd='/workspace',stdout=open(root/'browser.log','w'),stderr=subprocess.STDOUT)
(root/'process.json').write_text(json.dumps({'launcherPID':p.pid,'product':str(product),'profile':str(profile)}))
for i in range(90):
 try:m=Marionette('127.0.0.1',port);break
 except Exception:time.sleep(1)
else:raise RuntimeError('no browser')
time.sleep(5);(root/'ready').write_text('ready')
while True:
 cmd=root/'command.json'
 if cmd.exists():
  r=json.loads(cmd.read_text());cmd.unlink()
  try:
   m.call('Marionette:SetContext',{'value':r.get('context','content')})
   if r.get('click'):m.click(r['click']);time.sleep(1)
   if r.get('point'):
    x,y=r['point'];m.call('WebDriver:PerformActions',{'actions':[{'type':'pointer','id':'mouse','parameters':{'pointerType':'mouse'},'actions':[{'type':'pointerMove','duration':0,'origin':'viewport','x':int(x),'y':int(y)},{'type':'pointerDown','button':r.get('button',0)},{'type':'pointerUp','button':r.get('button',0)}]}]});time.sleep(1)
   if r.get('clickShadow'):
    el=m.script(r['clickShadow']);eid=el['element-6066-11e4-a52e-4f735466cecf'];m.call('WebDriver:ElementClick',{'id':eid});time.sleep(1)
   if r.get('call'):m.call(r['call']['name'],r['call'].get('args',{}));time.sleep(1)
   v=m.script(r.get('script','return true;'))
   if r.get('screenshot'):(root/r['screenshot']).write_bytes(base64.b64decode(m.call('WebDriver:TakeScreenshot',{'full':False})['value']))
   if r.get('quit'):
    v={'value':v,'normalQuit':m.call('Marionette:Quit',{'flags':['eAttemptQuit']})};p.wait(20)
    after=inventory(product);(root/'inventory-after.json').write_text(json.dumps(after));v['productUnchanged']=after==json.loads((root/'inventory-before.json').read_text())
   (root/r['out']).write_text(json.dumps(v,indent=2))
   if r.get('quit'):display_process.terminate();display_process.wait(10);break
  except Exception as e:(root/r['out']).write_text(json.dumps({'error':str(e),'trace':traceback.format_exc()}))
 time.sleep(.2)
