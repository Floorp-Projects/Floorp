#!/usr/bin/env python3
"""Run the new exact colocated test body in a disposable packaged product.
Synthetic privileged test automation; no sandbox or enrollment overrides.
"""
import argparse
import json
from pathlib import Path
from pwa_ui_probe import Marionette

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port',type=int,default=2839)
parser.add_argument('--profile',type=Path,required=True)
parser.add_argument('--out',type=Path,required=True)
args=parser.parse_args()
client=Marionette('127.0.0.1',args.port)
result={}
try:
    result['observedProfile']=client.script("return Services.dirsvc.get('ProfD',Ci.nsIFile).path;")
    if Path(result['observedProfile']).resolve()!=args.profile.resolve():
        raise RuntimeError('Wrong browser profile; refusing control')
    result['sandbox']=client.script("return Services.prefs.getIntPref('security.sandbox.content.level');")
    bundle=Path('/workspace/qa-linux-22/package-pwa/pwa-colocated-regression.js').read_text()
    result['test']=client.async_script(bundle+r"""
const done=arguments[arguments.length-1];
qaPwaTests.testLinuxWindowsKeepDistinctLauncherClasses().then(
 ()=>done({passed:true}),error=>done({passed:false,error:String(error),stack:error.stack}));
""")
    result['quit']=client.call('Marionette:Quit',{'flags':['eAttemptQuit']})
finally:
    client.socket.close()
    args.out.write_text(json.dumps(result,indent=2))
print(json.dumps(result))
