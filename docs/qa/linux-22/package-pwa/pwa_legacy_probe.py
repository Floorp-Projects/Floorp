#!/usr/bin/env python3
"""Exercise actual packaged Linux helper/window APIs with legacy malformed records.
Uses a fresh dedicated profile; no store seeding, network sites or rollout changes.
"""
import argparse
import json
from pathlib import Path
from pwa_ui_probe import Marionette

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port',type=int,default=2842)
parser.add_argument('--profile',type=Path,required=True)
parser.add_argument('--out',type=Path,required=True)
args=parser.parse_args()
client=Marionette('127.0.0.1',args.port)
result={}
try:
    profile=client.script("return Services.dirsvc.get('ProfD',Ci.nsIFile).path;")
    if Path(profile).resolve()!=args.profile.resolve():
        raise RuntimeError('Wrong dedicated profile')
    result['profile']=profile
    client.call('WebDriver:SetTimeouts',{'script':120000})
    result['calls']=client.async_script(r"""
const done=arguments[arguments.length-1];
(async()=>{
 const {LinuxSupport}=ChromeUtils.importESModule('resource://noraneko/modules/pwa/supports/Linux.sys.mjs');
 const {SsbRunnerUtils}=ChromeUtils.importESModule('resource://noraneko/modules/pwa/SsbCommandLineHandler.sys.mjs');
 const {TaskbarExperiment}=ChromeUtils.importESModule('resource://noraneko/modules/pwa/TaskbarExperiment.sys.mjs');
 const cases=[
  {label:'valid-braced-uuid',app:{id:'{01234567-89ab-4cde-8123-456789abcdef}',name:'Legacy Valid',start_url:'about:blank',icon:''}},
  {label:'missing-id',app:{name:'Legacy Missing ID',start_url:'about:blank',icon:''}},
  {label:'brace-only-id',app:{id:'{}',name:'Legacy Braces',start_url:'about:blank',icon:''}},
  {label:'punctuation-id',app:{id:' < > ',name:'Legacy Punctuation',start_url:'about:blank',icon:''}},
  {label:'uppercase-id',app:{id:'{ABCD-1234}',name:'Legacy Uppercase',start_url:'about:blank',icon:''}},
  {label:'missing-id-name',app:{start_url:'about:blank',icon:''}},
  {label:'numeric-id',app:{id:123,name:'Legacy Number',start_url:'about:blank',icon:''}},
  {label:'object-id',app:{id:{invalid:true},name:'Legacy Object',start_url:'about:blank',icon:''}},
  {label:'no-normalizable-fields',app:{id:'{}',name:'',start_url:'',icon:''}}
 ];
 const output=[];
 for(const {label,app} of cases){
  const row={label,app};
  try {const paths=LinuxSupport.getPathInfo(app);row.launcherClass=paths.startupWMClass;}
  catch(e){row.launcherError=String(e);}
  let win;
  try {
   win=await SsbRunnerUtils.openSsbWindow(app);
   row.opened=true;
   row.windowClass=win.document.documentElement.getAttribute('windowclass');
   row.taskbarId=win.document.documentElement.getAttribute('taskbartab');
  } catch(e){row.opened=false;row.openError=String(e);}
  finally {if(win&&!win.closed)win.close();}
  output.push(row);
 }
 return {cases:output,experiment:TaskbarExperiment.isEnabledForLinux(),sandbox:Services.prefs.getIntPref('security.sandbox.content.level')};
})().then(done,error=>done({harnessError:String(error),stack:error.stack}));
""")
    result['quit']=client.call('Marionette:Quit',{'flags':['eAttemptQuit']})
finally:
    client.socket.close()
    args.out.write_text(json.dumps(result,indent=2))
print(json.dumps(result))
