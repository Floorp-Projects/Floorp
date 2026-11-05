#!/usr/bin/env python3
"""Dedicated normally configured prefix-install KDE session; no permission bypass."""
from pathlib import Path
import os,sys,json,subprocess,time,signal,shutil
B=Path(__file__).resolve().parents[2]; D=Path(__file__).resolve().parent; R=B/'sysroot'
HOME=D/'home'; RUN=D/'runtime'; STATE=D/'session.json'
def config():
 HOME.mkdir(exist_ok=True);RUN.mkdir(exist_ok=True);RUN.chmod(0o700)
 data=HOME/'.local/share';services=data/'dbus-1/services';services.mkdir(parents=True,exist_ok=True)
 # Relocate existing activation executables, preserving all service arguments.
 for p in (R/'usr/share/dbus-1/services').glob('*.service'):
  lines=p.read_text().splitlines();changed=[]
  for s in lines:
   if s.startswith('Exec='):
    value=s[5:];exe=value.split(' ',1)[0];replacement=R/exe.lstrip('/')
    if exe.startswith('/usr/') and replacement.is_file():s='Exec='+str(replacement)+value[len(exe):]
   changed.append(s)
  (services/p.name).write_text('\n'.join(changed)+'\n')
 e=dict(HOME=str(HOME),XDG_DATA_HOME=str(data),XDG_CONFIG_HOME=str(HOME/'.config'),XDG_CACHE_HOME=str(HOME/'.cache'),XDG_RUNTIME_DIR=str(RUN),DISPLAY=':91',WAYLAND_DISPLAY='qa-followup48',QT_QPA_PLATFORM='wayland',XDG_CURRENT_DESKTOP='KDE',KDE_FULL_SESSION='true',KDE_SESSION_VERSION='6',DESKTOP_SESSION='plasmawayland',XDG_MENU_PREFIX='plasma-',XDG_DATA_DIRS=str(data)+':'+str(R/'usr/share')+':/usr/share',XDG_CONFIG_DIRS=str(R/'etc/xdg'),QT_PLUGIN_PATH=str(R/'usr/lib/x86_64-linux-gnu/qt6/plugins'),QML2_IMPORT_PATH=str(R/'usr/lib/x86_64-linux-gnu/qt6/qml'),LD_LIBRARY_PATH=str(R/'usr/lib/x86_64-linux-gnu'),LIBGL_DRIVERS_PATH=str(R/'usr/lib/x86_64-linux-gnu/dri'),__EGL_VENDOR_LIBRARY_FILENAMES=str(R/'usr/share/glvnd/egl_vendor.d/50_mesa.json'),PIPEWIRE_CONFIG_DIR=str(R/'usr/share/pipewire'),PATH=str(R/'usr/bin')+':/usr/bin:/bin',QT_LOGGING_RULES='kwin_core.debug=true;kwin_wayland_server.debug=true')
 return e
def env(e):
 out=dict(os.environ);out.update(e)
 assert 'KWIN_WAYLAND_NO_PERMISSION_CHECKS' not in out
 out.pop('MOZ_DISABLE_CONTENT_SANDBOX',None)
 return out
def launch(cmd,e,log):
 with (D/log).open('w') as f:return subprocess.Popen(cmd,env=env(e),stdout=f,stderr=subprocess.STDOUT).pid
def read():return json.loads(STATE.read_text())
action=sys.argv[1]
if action=='start':
 e=config();pids={}
 if '--baseline' in sys.argv[2:]:
  (HOME/'.local/share/applications/org.kde.plasmashell.desktop').unlink(missing_ok=True)
 pids['xvfb']=launch([str(R/'usr/bin/Xvfb'),':91','-screen','0','1280x900x24','-nolisten','tcp'],e,'xvfb.log');time.sleep(1)
 out=subprocess.check_output(['/usr/bin/dbus-daemon','--session','--fork','--print-address=1','--print-pid=1','--address=unix:path='+str(RUN/'bus')],env=env(e),text=True).splitlines();e['DBUS_SESSION_BUS_ADDRESS']=out[0];pids['bus']=int(out[1])
 subprocess.run([str(R/'usr/bin/kbuildsycoca6'),'--noincremental'],env=env(e),stdout=(D/'sycoca-baseline.log').open('w'),stderr=subprocess.STDOUT,check=True)
 pids['kwin']=launch([str(R/'usr/bin/kwin_wayland'),'--x11-display',':91','--socket','qa-followup48','--xwayland','--no-lockscreen','--width','1100','--height','760'],e,'kwin.log')
 for _ in range(100):
  if (RUN/'qa-followup48').exists():break
  time.sleep(.2)
 pids['plasma']=launch([str(R/'usr/bin/plasmashell')],dict(e,WAYLAND_DEBUG='client'),'plasma-baseline.log')
 STATE.write_text(json.dumps({'env':e,'pids':pids},indent=2)+'\n');print(json.dumps({'pids':pids,'permissionBypass':False}),flush=True)
 # Keep the owned desktop supervisor alive in the execution session.
 os.waitpid(pids['kwin'],0)
elif action=='exec':
 os.execvpe(sys.argv[2],sys.argv[2:],env(read()['env']))
elif action=='repair-prefix':
 s=read();e=s['env'];src=R/'usr/share/applications/org.kde.plasmashell.desktop';original=src.read_text();new=original.replace('Exec=/usr/bin/plasmashell','Exec='+str(R/'usr/bin/plasmashell'))
 assert new!=original
 target=HOME/'.local/share/applications/org.kde.plasmashell.desktop';target.parent.mkdir(parents=True,exist_ok=True);target.write_text(new)
 assert [l for l in original.splitlines() if l.startswith('X-KDE-Wayland-Interfaces=')]==[l for l in new.splitlines() if l.startswith('X-KDE-Wayland-Interfaces=')]
 subprocess.run([str(R/'usr/bin/kbuildsycoca6'),'--noincremental'],env=env(e),stdout=(D/'sycoca-repaired.log').open('w'),stderr=subprocess.STDOUT,check=True)
 s['pids']['plasma']=launch([str(R/'usr/bin/plasmashell'),'--replace'],dict(e,WAYLAND_DEBUG='client'),'plasma-repaired.log');STATE.write_text(json.dumps(s,indent=2)+'\n')
 (D/'prefix-repair.json').write_text(json.dumps({'originalExec':'/usr/bin/plasmashell','correctedExec':str(R/'usr/bin/plasmashell'),'interfaceDeclarationsUnchanged':True,'permissionCheckBypass':False},indent=2)+'\n')
 print('Corrected only the existing desktop Exec prefix; original interface declarations retained.',flush=True)
 os.waitpid(s['pids']['plasma'],0)
elif action=='stop':
 s=read();out=[]
 for kind in ['plasma','kwin','xvfb','bus']:
  pid=s['pids'][kind]
  try:
   args=Path(f'/proc/{pid}/cmdline').read_bytes().decode().split('\0')
   allowed=bool(args and (args[0].startswith(str(R)+'/') or (kind=='bus' and args[0].endswith('dbus-daemon') and '--address=unix:path='+str(RUN/'bus') in args)))
   if allowed:os.kill(pid,signal.SIGTERM)
   out.append({'kind':kind,'pid':pid,'ownedCommandMatched':allowed})
  except (FileNotFoundError,ProcessLookupError):out.append({'kind':kind,'pid':pid,'alreadyExited':True})
 # Match only this follow-up's standard control and fixture server.
 for p in Path('/proc').iterdir():
  if not p.name.isdigit():continue
  try:
   a=(p/'cmdline').read_bytes().decode().split('\0')
   if (a and a[0]==str(R/'usr/bin/kwrite') and str(D/'standard-app-control.txt') in a) or (str(D/'serve.py') in a and '8786' in a):
    os.kill(int(p.name),signal.SIGTERM);out.append({'kind':'owned-control-or-fixture','pid':int(p.name)})
  except (FileNotFoundError,ProcessLookupError,PermissionError,UnicodeError):pass
 (D/'cleanup.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out))
