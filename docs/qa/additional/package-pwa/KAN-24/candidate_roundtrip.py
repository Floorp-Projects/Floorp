import sys,json,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent));from run_steps import step
snapshot=Path('/workspace/qa-linux-additional/KAN-24/product-baseline/snapshot.js').read_text()
step('04-hub-vertical',click='input[name=style][value=vertical]',script='return document.querySelector("input[name=style]:checked").value',screenshot='04-hub-vertical.png')
step('05-vertical-render',context='chrome',script=snapshot)
step('06-nav-native',script="location.href='about:preferences#tabsBrowsing';return true;")
time.sleep(3)
for i in range(30):
 r=step(f'07-native-ready-{i}',script="const e=document.getElementById('browserLayoutHorizontalTabs');if(!e)return null;const b=e.shadowRoot?.querySelector('button');if(!b)return null;b.scrollIntoView({block:'center'});return b.getBoundingClientRect().toJSON();")
 if r and r['width']>0:break
 time.sleep(.5)
else:raise RuntimeError('native unavailable')
step('08-native-off',point=[r['x']+r['width']/2,r['y']+r['height']/2],script="return {horizontal:document.getElementById('browserLayoutHorizontalTabs').checked,vertical:document.getElementById('browserLayoutVerticalTabs').checked}",screenshot='08-native-off.png')
step('09-native-off-render',context='chrome',script=snapshot)
step('10-back-hub',script="location.href='about:hub#/features/design';return true;")
time.sleep(3)
step('11-hub-after-off',script="return {design:document.querySelector('input[name=design]:checked')?.value,style:document.querySelector('input[name=style]:checked')?.value}",screenshot='11-hub-after-off.png')
step('12-multirow',click='input[name=style][value=multirow]',script="return document.querySelector('input[name=style]:checked').value",screenshot='12-multirow.png')
step('13-multirow-render',context='chrome',script=snapshot)
step('14-workspaces-open',click='a[href="/features/workspaces"]',script="return {text:document.body.innerText,inputs:Array.from(document.querySelectorAll('input,button'),e=>({tag:e.tagName,id:e.id,type:e.type,text:e.textContent,name:e.name,value:e.value,checked:e.checked}))}")
