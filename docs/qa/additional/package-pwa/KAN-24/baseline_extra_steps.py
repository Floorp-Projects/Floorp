import sys,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent));from run_steps import step
snap=Path('/workspace/qa-linux-additional/KAN-24/product-baseline/snapshot.js').read_text()
ready="return Array.from(Services.wm.getEnumerator('navigator:browser'),w=>({ready:w.gBrowserInit.delayedStartupFinished,vertical:w.gBrowser.tabContainer.verticalMode,rect:w.gBrowser.tabContainer.getBoundingClientRect().toJSON(),orient:w.gBrowser.tabContainer.getAttribute('orient'),url:w.gBrowser.selectedBrowser.currentURI.spec}));"
step('01-before',context='chrome',script=snap)
step('02-newwindow-menu',context='chrome',click='#PanelUI-menu-button',script='return true;')
step('03-newwindow-click',context='chrome',click='#appMenu-new-window-button2',script='return true;')
for i in range(60):
 v=step(f'04-ready-{i}',context='chrome',script=ready)
 if len(v)==2 and all(w['ready']and w['url']!='about:blank'for w in v):break
 time.sleep(.5)
step('05-newwindow-settled',context='chrome',script=snap,screenshot='05-newwindow-settled.png')
step('06-workspace-popup',context='chrome',click='#workspaces-toolbar-button',script="return Array.from(document.querySelectorAll('.workspaceButton'),e=>({id:e.id,label:e.getAttribute('label')}));")
step('07-workspace-create',context='chrome',click='#workspacesCreateNewWorkspaceButton',script='return true;')
step('08-workspace-popup',context='chrome',click='#workspaces-toolbar-button',script="return {active:document.getElementById('workspaces-toolbar-button').getAttribute('label'),items:Array.from(document.querySelectorAll('.workspaceButton'),e=>({id:e.id,label:e.getAttribute('label')}))};")
step('09-workspace-first',context='chrome',click='.workspaceButton:first-child',script=snap,screenshot='09-workspace-first.png')
step('10-workspace-popup',context='chrome',click='#workspaces-toolbar-button',script='return true;')
step('11-workspace-second',context='chrome',click='.workspaceButton:nth-child(2)',script=snap,screenshot='11-workspace-second.png')
step('12-workspace-popup',context='chrome',click='#workspaces-toolbar-button',script='return true;')
step('13-workspace-return',context='chrome',click='.workspaceButton:first-child',script=snap)
step('14-original-tab',context='chrome',script='gBrowser.selectedTab=gBrowser.tabs[0];return true;')
step('15-hub-route',script="location.href='about:hub#/features/design';return true;")
for i in range(60):
 v=step(f'16-hub-ready-{i}',script="return {style:document.querySelector('input[name=style]:checked')?.value,design:document.querySelector('input[name=design]:checked')?.value};")
 if v.get('style'):break
 time.sleep(.5)
step('17-hub-after-workspace',script="return {style:document.querySelector('input[name=style]:checked')?.value,design:document.querySelector('input[name=design]:checked')?.value};",screenshot='17-hub-after-workspace.png')
step('18-normal-quit',context='chrome',script=snap,quit=True)
