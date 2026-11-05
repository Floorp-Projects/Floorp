import sys,json,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent));from run_steps import step
snapshot=Path('/workspace/qa-linux-additional/KAN-24/product-baseline/snapshot.js').read_text()
step('23-workspace-first',context='chrome',click='.workspaceButton:first-child',script=snapshot,screenshot='23-workspace-first.png')
step('24-workspace-reopen',context='chrome',click='#workspaces-toolbar-button',script='return true')
step('25-workspace-second',context='chrome',click='.workspaceButton:nth-child(2)',script=snapshot,screenshot='25-workspace-second.png')
step('26-workspace-reopen',context='chrome',click='#workspaces-toolbar-button',script='return true')
step('27-workspace-return',context='chrome',click='.workspaceButton:first-child',script=snapshot)
# close popup using genuine toolbar button if still open, navigation into the existing first tab
step('28-hub-return',context='chrome',script="gBrowser.selectedTab=gBrowser.tabs[0];return true;")
step('29-hub-route',script="location.href='about:hub#/features/design';return true;")
time.sleep(2)
step('30-multirow-preserved',script="return {design:document.querySelector('input[name=design]:checked')?.value,style:document.querySelector('input[name=style]:checked')?.value}",screenshot='30-multirow-preserved.png')
# native vertical-on action from multirow
step('31-native-on-route',script="location.href='about:preferences#tabsBrowsing';return true;")
time.sleep(2)
r=step('32-native-on-point',script="const e=document.getElementById('browserLayoutVerticalTabs').shadowRoot.querySelector('button');e.scrollIntoView({block:'center'});return e.getBoundingClientRect().toJSON();")
step('33-native-on',point=[r['x']+r['width']/2,r['y']+r['height']/2],script="return document.getElementById('browserLayoutVerticalTabs').checked")
step('34-native-on-render',context='chrome',script=snapshot)
step('35-hub-return',script="location.href='about:hub#/features/design';return true;")
time.sleep(2)
step('36-hub-after-native-on',script="return {design:document.querySelector('input[name=design]:checked')?.value,style:document.querySelector('input[name=style]:checked')?.value}",screenshot='36-hub-after-native-on.png')
# real normal-new-window UI menu command is invisible menu; native command via normal Ctrl+N later
step('37-normal-quit',context='chrome',script=snapshot,quit=True)
