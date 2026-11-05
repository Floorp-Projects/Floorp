import json,time,sys,subprocess
from pathlib import Path
root=Path(sys.argv[1]);counter=0
def step(name,**r):
 r['out']=name+'.json';out=root/r['out'];(root/'command.json').write_text(json.dumps(r))
 for _ in range(300):
  if out.exists():
   v=json.loads(out.read_text());print(name,json.dumps(v)[:600],flush=True)
   if isinstance(v,dict) and 'error'in v:raise RuntimeError(v)
   return v
  time.sleep(.2)
 raise TimeoutError(name)
if __name__=='__main__':
 step('01-photon',click='label:has(input[name=design][value=photon])',script='return document.querySelector("input[name=design]:checked").value')
 step('02-vertical',click='input[name=style][value=vertical]',script='return document.querySelector("input[name=style]:checked").value',screenshot='02-vertical.png')
 step('03-render',context='chrome',script=Path('/workspace/qa-linux-additional/KAN-24/product-baseline/snapshot.js').read_text())
 step('04-native-open',context='chrome',script="gBrowser.selectedTab=gBrowser.addTrustedTab('about:preferences#tabsBrowsing');return true;")
 time.sleep(3)
 step('05-layout',script="const e=document.getElementById('browserLayoutHorizontalTabs');return {pane:e.closest('setting-pane').outerHTML.slice(0,200),host:e.outerHTML,shadow:e.shadowRoot?.innerHTML,rect:e.getBoundingClientRect().toJSON()};")
