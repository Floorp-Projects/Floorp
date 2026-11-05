#!/usr/bin/env python3
from pathlib import Path
import os,subprocess,json,sys,time,socket,shutil
b=Path(__file__).resolve().parent;root=b.parent;home=b/'flatpak-x11-home-4';app=home/'.var/app/one.ablaze.floorp';profile=app/'config/branding-profile';profile.mkdir(parents=True,exist_ok=True)
(profile/'user.js').write_text('user_pref("marionette.port",2859);\nuser_pref("intl.locale.requested","en-US");\n')
for locale in ['es-CL','ja']:shutil.copyfile(b/(locale+'.xpi'),app/('config/'+locale+'.xpi'))
theme=app/'data/icons/qa-langpack';shutil.copytree(b/'home/.icons/qa-langpack',theme,dirs_exist_ok=True)
settings=app/'config/gtk-3.0';settings.mkdir(parents=True,exist_ok=True);(settings/'settings.ini').write_text('[Settings]\ngtk-icon-theme-name=qa-langpack\n')
env=dict(os.environ,HOME=str(home),XDG_DATA_HOME=str(root/'flatpak-home/.local/share'),XDG_CONFIG_HOME=str(home/'.config'),XDG_CACHE_HOME=str(home/'.cache'),XDG_RUNTIME_DIR=str(root/'runtime'),DISPLAY=':88',DBUS_SYSTEM_BUS_ADDRESS='unix:path='+str(root/'KAN-8/system-probe-bus'),FLATPAK_BWRAP='/usr/bin/bwrap',FLATPAK_DBUSPROXY=str(root/'sysroot/usr/bin/xdg-dbus-proxy'),LD_LIBRARY_PATH=str(root/'sysroot/usr/lib/x86_64-linux-gnu'),PATH=str(root/'sysroot/usr/bin')+':/usr/bin:/bin')
env.pop('MOZ_DISABLE_CONTENT_SANDBOX',None)
cmd=['dbus-run-session','--',str(root/'sysroot/usr/bin/flatpak'),'run','--user','--env=GDK_BACKEND=x11','--env=MOZ_ENABLE_WAYLAND=0','--env=MOZ_REMOTE_ALLOW_SYSTEM_ACCESS=1','--env=MOZ_MARIONETTE_PREF_STATE_ACROSS_RESTARTS='+json.dumps({'marionette.port':2859}),'one.ablaze.floorp','--profile',str(profile),'--marionette','about:blank']
sys.path.insert(0,str(root));from pwa_ui_probe import Marionette
for locale in ['en-US','es-CL']:
 (profile/'user.js').write_text('user_pref("marionette.port",2859);\nuser_pref("intl.locale.requested",'+json.dumps(locale)+');\n')
 with (b/('flatpak-'+locale+'-x11-browser.log')).open('w') as log:
  p=subprocess.Popen(cmd,env=env,stdout=log,stderr=subprocess.STDOUT)
  try:
   for _ in range(200):
    if p.poll() is not None:raise RuntimeError(f'early exit {p.returncode}')
    try:
     with socket.create_connection(('127.0.0.1',2859),.2):break
    except OSError:time.sleep(.2)
   time.sleep(3)
   probeenv=dict(env,QA_BRANDING_CLASS='one.ablaze.floorp',QA_BRANDING_WINDOW_NAME='^Ablaze Floorp$',QA_BRANDING_PORT='2859',QA_BRANDING_PROFILE=str(profile),QA_BRANDING_XPI_DIR=str(app/'config'))
   m=Marionette('127.0.0.1',2859)
   print('display-env',m.script("return {display:Services.env.get('DISPLAY'),wayland:Services.env.get('WAYLAND_DISPLAY'),gdk:Services.env.get('GDK_BACKEND'),mozWayland:Services.env.get('MOZ_ENABLE_WAYLAND'),pid:Services.appinfo.processID};"));m.call('WebDriver:DeleteSession');m.socket.close()
   tree=subprocess.run([str(root/'sysroot/usr/bin/xwininfo'),'-root','-tree'],env=env,capture_output=True,text=True);(b/('flatpak-'+locale+'-tree.txt')).write_text(tree.stdout+tree.stderr)
   args=[sys.executable,str(b/'probe.py'),'flatpak-'+locale+'-x11']
   if locale=='en-US':args+=['install']
   r=subprocess.run(args,env=probeenv,text=True,capture_output=True)
   (b/('flatpak-'+locale+'-x11-probe.log')).write_text(r.stdout+r.stderr);print(r.stdout+r.stderr)
   if r.returncode:raise RuntimeError('probe failed')
   m=Marionette('127.0.0.1',2859);actual=m.script("return Services.dirsvc.get('ProfD',Ci.nsIFile).path;");assert actual==str(profile),actual
   print(m.call('Marionette:Quit',{'flags':['eAttemptQuit']}));m.socket.close();print('exit',p.wait(timeout=30))
  finally:
   if p.poll() is None:p.terminate();p.wait(timeout=10)
