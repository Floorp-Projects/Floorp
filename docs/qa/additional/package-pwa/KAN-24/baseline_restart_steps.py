import sys,json,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent));from run_steps import step
snapshot=Path('/workspace/qa-linux-additional/KAN-24/product-baseline/snapshot.js').read_text()
step('01-restart-before-hub',context='chrome',script=snapshot)
step('02-open-hub',script="location.href='about:hub#/features/design';return true;")
for i in range(60):
 v=step(f'03-hub-ready-{i}',script="return {url:location.href,design:document.querySelector('input[name=design]:checked')?.value,style:document.querySelector('input[name=style]:checked')?.value}")
 if v.get('style'):break
 time.sleep(.5)
else:raise RuntimeError('Hub not ready')
step('04-restart-hub',script="return {url:location.href,design:document.querySelector('input[name=design]:checked')?.value,style:document.querySelector('input[name=style]:checked')?.value}",screenshot='04-restart-hub.png')
step('05-restart-ready-geometry',context='chrome',script="return Array.from(Services.wm.getEnumerator('navigator:browser'),w=>({ready:w.gBrowserInit.delayedStartupFinished,vertical:w.gBrowser.tabContainer.verticalMode,rect:w.gBrowser.tabContainer.getBoundingClientRect().toJSON(),orient:w.gBrowser.tabContainer.getAttribute('orient')}));")
step('06-normal-quit',context='chrome',script=snapshot,quit=True)
