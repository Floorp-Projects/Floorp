#!/usr/bin/env python3
"""Cold-launch two actual candidate PWA desktop Exec commands on nested Wayland.
Observes own profile/PID only; never controls the separate official KDE run.
"""
import http.server
import json
import os
from pathlib import Path
import shlex
import subprocess
import threading
import time
from pwa_ui_probe import Fixture, Marionette

root=Path(__file__).resolve().parent
out=root/'candidate-kde'
profile=out/'profile'
entries=json.loads((out/'desktop-correspondence.json').read_text())
entries.sort(key=lambda item:item['name'])
wrapper=['python3',str(root/'kde-exec.py')]
prefix=wrapper+['env','-u','MOZ_DISABLE_CONTENT_SANDBOX','MOZ_REMOTE_ALLOW_SYSTEM_ACCESS=1',
    'MOZ_ENABLE_WAYLAND=1','GDK_BACKEND=wayland','WAYLAND_DEBUG=1']
server=http.server.ThreadingHTTPServer(('127.0.0.1',8776),Fixture)
threading.Thread(target=server.serve_forever,daemon=True).start()
processes=[]
logs=[]
result={'kind':'isolated patched product copy, native Wayland cold CLI test',
    'commands':[],'expectedProfile':str(profile)}
client=None
try:
    for index,entry in enumerate(entries):
        command=shlex.split(entry['exec'])
        if index==0:command.append('--marionette')
        result['commands'].append(command)
        log=(out/f'cold-{index}-wayland.log').open('wb');logs.append(log)
        process=subprocess.Popen(prefix+command,stdout=log,stderr=subprocess.STDOUT)
        processes.append(process)
        time.sleep(12 if index==0 else 7)
    client=Marionette('127.0.0.1',2844)
    result['product']=client.script(r"""
return {pid:Services.appinfo.processID,profile:Services.dirsvc.get('ProfD',Ci.nsIFile).path,
 sandbox:Services.prefs.getIntPref('security.sandbox.content.level'),
 experiment:ChromeUtils.importESModule('resource://noraneko/modules/pwa/TaskbarExperiment.sys.mjs').TaskbarExperiment.isEnabledForLinux(),
 windows:Array.from(Services.wm.getEnumerator('navigator:browser'),w=>({title:w.document.title,
 class:w.document.documentElement.getAttribute('windowclass'),url:w.gBrowser?.selectedBrowser?.currentURI?.spec}))};
""")
    if Path(result['product']['profile']).resolve()!=profile.resolve():
        raise RuntimeError('Wrong profile; refusing control')
    subprocess.run(wrapper+['sh',str(out/'observe-kde.sh')],check=True,timeout=20,capture_output=True,text=True)
    marker='QA_KAN49_CANDIDATE_WINDOWS='
    lines=[line for line in (root/'kwin-nested.log').read_text().splitlines() if marker in line]
    windows=json.loads(lines[-1].split(marker,1)[1])
    result['kwinWindows']=[w for w in windows if w['pid']==result['product']['pid']]
    result['quit']=client.call('Marionette:Quit',{'flags':['eAttemptQuit']})
    result['completed']=True
except Exception as error:
    result['completed']=False;result['error']=str(error)
finally:
    if client:client.socket.close()
    for process in processes:
        try:process.wait(timeout=15)
        except subprocess.TimeoutExpired:
            # Only the owned process; separate official KDE product is untouched.
            result.setdefault('terminationFallback',[]).append(process.pid)
            process.terminate();process.wait(timeout=15)
    for log in logs:log.close()
    server.shutdown()
    (out/'cold-launch-result.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result))
