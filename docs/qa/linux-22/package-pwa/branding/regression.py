#!/usr/bin/env python3
import sys,json,hashlib,base64,time
from pathlib import Path
sys.path.insert(0,'/workspace/qa-linux-22/package-pwa');from pwa_ui_probe import Marionette
b=Path(__file__).resolve().parent;m=Marionette('127.0.0.1',2829)
profile=m.script("return Services.dirsvc.get('ProfD',Ci.nsIFile).path;");assert profile==str(b/'profile'),profile
src=Path('/workspace/qa-linux-branding-fix/browser-features/modules/modules/i18n/test/LegacyBranding.test.mts').read_text()
start=src.index('fn() {')+len('fn() {');end=src.index('\n      },',start);body=src[start:end]
result=m.script('''const rows=[];function assertEquals(a,b,message){rows.push({actual:a,expected:b,message,pass:a===b});if(a!==b)throw Error(message+': '+a+' != '+b);}try{'''+body+''';return {pass:true,rows};}catch(e){return {pass:false,rows,error:String(e)}}''')
result.update({'profile':profile,'testFileSha256':hashlib.sha256(src.encode()).hexdigest(),'execution':'exact fn body extracted from colocated browser regression; assertion callback records comparisons; standard colocated runner not used','locale':m.script('return Services.locale.appLocaleAsBCP47;')})
try:
 m.click('#PanelUI-menu-button');time.sleep(.5)
 result['menu']=m.script("return ['appMenu-settings-button','appMenu-new-window-button2'].map(id=>({id,label:document.getElementById(id)?.label,text:document.getElementById(id)?.textContent}));")
 data=m.call('WebDriver:TakeScreenshot',{'full':True});(b/(sys.argv[1]+'-menu.png')).write_bytes(base64.b64decode(data.get('value',data)))
except Exception as e:result['uiProbeError']=str(e)
(b/(sys.argv[1]+'-regression.json')).write_text(json.dumps(result,indent=2,ensure_ascii=False));print(json.dumps(result,ensure_ascii=False));m.call('WebDriver:DeleteSession')
