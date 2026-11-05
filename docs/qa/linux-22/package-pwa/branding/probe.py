#!/usr/bin/env python3
import sys,json,base64,subprocess,time,hashlib,os
from pathlib import Path
sys.path.insert(0,'/workspace/qa-linux-22/package-pwa')
from pwa_ui_probe import Marionette
b=Path(__file__).resolve().parent
m=Marionette('127.0.0.1',int(os.environ.get('QA_BRANDING_PORT','2829')))
actual=m.script("return Services.dirsvc.get('ProfD',Ci.nsIFile).path;")
assert actual == os.environ.get('QA_BRANDING_PROFILE',str(b/'profile')), actual
label=sys.argv[1]
state=m.script('''const win=Services.wm.getMostRecentWindow('navigator:browser');const app=Services.appinfo;return {version:app.version,platformVersion:app.platformVersion,pid:app.processID,locale:Services.locale.appLocaleAsBCP47,requested:Services.locale.requestedLocales,brand:Services.strings.createBundle('chrome://branding/locale/brand.properties').GetStringFromName('brandShortName'),uri:Services.io.newURI('chrome://branding/locale/brand.properties').spec,sandbox:Services.prefs.getIntPref('security.sandbox.content.level'),title:win.document.title};''')
state['profile']=actual
state['addons']=m.async_script('''const done=arguments[arguments.length-1];ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs').AddonManager.getAddonsByTypes(['locale']).then(xs=>done(xs.map(a=>({id:a.id,version:a.version,isActive:a.isActive,signedState:a.signedState,userDisabled:a.userDisabled}))));''')
search=['--pid',str(state['pid'])]
windows=subprocess.check_output(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool','search',*search],text=True).split()
state['windows']=[]
for wid in windows:
 p=subprocess.run(['xprop','-id',wid,'WM_CLASS','_NET_WM_NAME','_NET_WM_ICON'],capture_output=True,text=True)
 (b/(label+'-'+wid+'-xprop.txt')).write_text(p.stdout)
 state['windows'].append({'id':wid,'xpropSha256':hashlib.sha256(p.stdout.encode()).hexdigest(),'iconPresent':'_NET_WM_ICON(CARDINAL)' in p.stdout})
try:
 data=m.call('WebDriver:TakeScreenshot',{'full':True});data=data.get('value',data);(b/(label+'.png')).write_bytes(base64.b64decode(data))
except Exception as e:state['screenshotError']=str(e)
(b/(label+'.json')).write_text(json.dumps(state,indent=2));print(json.dumps(state))
if len(sys.argv)>2 and sys.argv[2]=='install':
 for locale in ['es-CL','ja']:
  result=m.async_script('''const done=arguments[arguments.length-1];const {AddonManager}=ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs');AddonManager.getInstallForURL(arguments[0]).then(i=>{i.addListener({onInstallEnded(_i,a){done({id:a.id,signedState:a.signedState,active:a.isActive});},onDownloadFailed(i){done({error:i.error,state:i.state});},onInstallFailed(i){done({error:i.error,state:i.state});}});i.install();}).catch(e=>done({error:String(e)}));''',['file://'+str(Path(os.environ.get('QA_BRANDING_XPI_DIR',str(b)))/(locale+'.xpi'))])
  print('install',locale,result);(b/('install-'+locale+'.json')).write_text(json.dumps(result))
if len(sys.argv)>2 and sys.argv[2]=='set-locale':
 print(m.script('Services.locale.requestedLocales=[arguments[0]];return Services.locale.requestedLocales;',[sys.argv[3]]))
m.call('WebDriver:DeleteSession')
