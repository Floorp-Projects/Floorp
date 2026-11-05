#!/usr/bin/env python3
"""Run generated Linux PWA launchers in all three requested process-first orders.
Synthetic CLI startup/X11 observation; no physical-input or KDE claim.
Run within dbus-run-session with the isolated product HOME/runtime/display/libs.
Requires the real UI-created native-profile/ssb/ssb.json and desktop entries.
"""
import argparse
import hashlib
import http.server
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import threading
import time
from pwa_ui_probe import Fixture, Marionette


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--port',type=int,default=2839)
    parser.add_argument('--binary',type=Path)
    parser.add_argument('--out',type=Path)
    args=parser.parse_args()
    root=args.root.resolve()
    out=args.out or root/'pwa-order-run'
    out.mkdir(exist_ok=True)
    env=dict(os.environ)
    if env.get('MOZ_DISABLE_CONTENT_SANDBOX'):
        raise RuntimeError('Refusing disabled content sandbox')
    env['MOZ_REMOTE_ALLOW_SYSTEM_ACCESS']='1'
    binary=args.binary or root/'product/floorp/floorp'
    profile=root/'native-profile'
    sysroot=root/'sysroot/usr/bin'
    commands={'browser':[str(binary),'--profile',str(profile),'about:blank']}
    desktop_metadata={}
    for path in (root/'home/.local/share/applications').glob('floorp-*.desktop'):
        values=dict(line.split('=',1) for line in path.read_text().splitlines() if '=' in line)
        label='alpha' if values['Name'].endswith('Alpha') else 'beta'
        commands[label]=shlex.split(values['Exec'])
        commands[label][0]=str(binary)
        icon=Path(values['Icon'])
        desktop_metadata[label]={'path':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
            'startupWMClass':values['StartupWMClass'],'exec':commands[label],
            'icon':str(icon),'iconSha256':hashlib.sha256(icon.read_bytes()).hexdigest()}
    server=http.server.ThreadingHTTPServer(('127.0.0.1',8765),Fixture)
    threading.Thread(target=server.serve_forever,daemon=True).start()
    results={'input':'synthetic CLI launch, actual generated desktop Exec, X11 properties',
        'desktop':'Xvfb X11; this is not a KDE taskbar test',
        'desktopEntries':desktop_metadata,'orders':[]}
    try:
        for order in [('alpha','beta','browser'),('beta','alpha','browser'),('browser','alpha','beta')]:
            label='-'.join(order)
            row={'order':order,'commands':[]}
            results['orders'].append(row)
            processes=[]
            logs=[]
            try:
                for index,app in enumerate(order):
                    command=commands[app].copy()
                    if index==0:
                        command.append('--marionette')
                    row['commands'].append(command)
                    log=(out/f'{label}-{app}.log').open('wb')
                    logs.append(log)
                    process=subprocess.Popen(command,env=env,stdout=log,stderr=subprocess.STDOUT)
                    processes.append(process)
                    time.sleep(7 if index==0 else 5)
                tree=subprocess.run([str(sysroot/'xwininfo'),'-root','-tree'],env=env,text=True,capture_output=True,timeout=15)
                (out/f'{label}-tree.txt').write_text(tree.stdout+tree.stderr)
                row['windows']=[]
                for line in tree.stdout.splitlines():
                    match=re.search(r'^\s+(0x[0-9a-f]+) "([^"]*)"',line)
                    if not match or not ('QA PWA' in match[2] or 'Ablaze Floorp' in match[2]):
                        continue
                    wid,title=match.groups()
                    props=subprocess.run([str(sysroot/'xprop'),'-id',wid,'WM_CLASS','_NET_WM_PID','_NET_WM_ICON'],
                        env=env,text=True,capture_output=True,timeout=15)
                    observed_pid=re.search(r'_NET_WM_PID\(CARDINAL\) = (\d+)',props.stdout)
                    if not observed_pid or int(observed_pid[1]) != processes[0].pid:
                        continue
                    propfile=out/f'{label}-{wid}-properties.txt'
                    propfile.write_text(props.stdout+props.stderr)
                    wm=re.search(r'WM_CLASS\(STRING\) = (.*)',props.stdout)
                    row['windows'].append({'xid':wid,'title':title,'wmClass':wm[1] if wm else None,
                        'propertyFile':propfile.name,'propertiesSha256':hashlib.sha256(propfile.read_bytes()).hexdigest()})
                client=Marionette('127.0.0.1',args.port)
                observed_profile=client.script("return Services.dirsvc.get('ProfD',Ci.nsIFile).path;")
                row['observedProfile']=observed_profile
                row['ownerPid']=processes[0].pid
                if Path(observed_profile).resolve()!=profile.resolve():
                    client.socket.close()
                    raise RuntimeError('Refusing to control a different browser profile: '+observed_profile)
                row['sandbox']=client.script("return Services.prefs.getIntPref('security.sandbox.content.level');")
                row['experiment']=client.script("return ChromeUtils.importESModule('resource://noraneko/modules/pwa/TaskbarExperiment.sys.mjs').TaskbarExperiment.isEnabledForLinux();")
                try:
                    row['quit']=client.call('Marionette:Quit',{'flags':['eAttemptQuit']})
                finally:
                    client.socket.close()
                row['completed']=True
            except Exception as error:
                row['completed']=False
                row['error']=str(error)
            finally:
                for process in processes:
                    try:
                        process.wait(timeout=15)
                    except subprocess.TimeoutExpired:
                        row.setdefault('terminationFallback',[]).append(process.pid)
                        process.terminate()
                        process.wait(timeout=15)
                for log in logs:
                    log.close()
                (out/'result.json').write_text(json.dumps(results,indent=2))
                time.sleep(1)
    finally:
        server.shutdown()
    print(json.dumps(results,indent=2))


if __name__=='__main__':
    main()
