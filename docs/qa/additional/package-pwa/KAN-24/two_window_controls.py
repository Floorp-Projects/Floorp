import sys,json,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent));from run_steps import step
r=Path(sys.argv[1]);handles=json.loads((r/'46-window-handles.json').read_text());a=handles[0]['tabs'][0]['handle'];b=handles[1]['tabs'][0]['handle']
def switch(name,handle):step(name,call={'name':'WebDriver:SwitchToWindow','args':{'handle':handle,'focus':True}},script='return location.href')
switch('49-switch-B',b)
step('50-hub-B',script="location.href='about:hub#/features/design';return true;");time.sleep(2)
step('51-B-initial',script="return document.querySelector('input[name=style]:checked')?.value")
step('52-B-multirow',click='input[name=style][value=multirow]',script="return document.querySelector('input[name=style]:checked').value")
switch('53-switch-A',a);time.sleep(1)
step('54-A-after-focus',script="return document.querySelector('input[name=style]:checked')?.value")
step('55-A-unrelated',click='label:has(#favicon-color)',script="return {style:document.querySelector('input[name=style]:checked')?.value,favicon:document.getElementById('favicon-color').checked}")
step('56-after-unrelated-render',context='chrome',script=Path('/workspace/qa-linux-additional/KAN-24/product-baseline/snapshot.js').read_text())
step('57-final-normal-quit',context='chrome',script="return {windows:Array.from(Services.wm.getEnumerator('navigator:browser'),w=>({ready:w.gBrowserInit.delayedStartupFinished,vertical:w.gBrowser.tabContainer.verticalMode})),native:Services.prefs.getBoolPref('sidebar.verticalTabs'),stored:JSON.parse(Services.prefs.getStringPref('floorp.design.configs')).tabbar.tabbarStyle}",quit=True)
