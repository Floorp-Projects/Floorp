#!/usr/bin/env python3
"""Project measured native PWA and ordinary-browser identities without fixing them."""
import argparse
import json
from pathlib import Path
import re

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--result',type=Path,required=True)
args=parser.parse_args()
r=json.loads(args.result.read_text());root=args.result.parent

def protocol(path):
    windows={}
    for line in path.read_text(errors='replace').splitlines():
        match=re.search(r'xdg_toplevel#(\d+)\.(set_app_id|set_title)\((".*")\)',line)
        if not match:continue
        window,method,value=match.groups()
        try:value=json.loads(value)
        except json.JSONDecodeError:value=value[1:-1]
        windows.setdefault(window,{'toplevel':window})[method]=value
    return list(windows.values())

projection={'sourceSha':r['sourceSha'],'runId':r['runId'],'mode':r['mode'],
    'completed':r['completed'],'pwaChecks':r.get('pwaIdentityChecks'),
    'expectedOrdinaryBrowserIdentity':'floorp',
    'ordinaryBrowserProductWindows':[w for w in r.get('coldProduct',{}).get('windows',[]) if w['url']=='about:blank'],
    'productUnchanged':r.get('productUnchanged'),'remainingOwnedProcesses':r.get('remainingOwnedProcesses')}
if r['mode']=='wayland':
    projection['browserFirstUiProtocol']=protocol(root/'ui-browser.log')
    projection['coldProtocol']=protocol(root/'cold-0.log')
    projection['ordinaryBrowserKwin']=[w for w in r.get('kwinWindows',[]) if w['caption']=='Ablaze Floorp']
    projection['coldOrdinaryBrowserProtocol']=[w for w in projection['coldProtocol'] if w.get('set_title')=='Ablaze Floorp']
    projection['ordinaryBrowserMatches']=bool(projection['ordinaryBrowserKwin']) and all(w['resourceClass']=='floorp' for w in projection['ordinaryBrowserKwin'])
    projection['expectedIdentityEvidence']='The browser-first UI phase advertises floorp before any generated PWA --name argument; cold ordinary browser URL is about:blank and has no windowclass override.'
else:
    projection['ordinaryBrowserX11']=[w for w in r.get('x11Windows',[]) if w['title']=='Ablaze Floorp']
    projection['ordinaryBrowserMatches']=bool(projection['ordinaryBrowserX11']) and all(w['wmClass']=='"Navigator", "floorp"' for w in projection['ordinaryBrowserX11'])
target=root/'native-identity-projection.json'
target.write_text(json.dumps(projection,indent=2,ensure_ascii=False))
print(json.dumps(projection,ensure_ascii=False))
