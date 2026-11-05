#!/usr/bin/env python3
"""Compare observed native identities to real launcher/CLI identities; no mutation."""
import argparse
import json
from pathlib import Path
import re

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--result',type=Path,required=True)
args=parser.parse_args();result=json.loads(args.result.read_text())
events=json.loads((args.result.parent/'native-protocol.json').read_text())
rows=[]
for case in result['orders']:
    migrated=case['case'] in ['existing-profile-migration','custom-browser-remote-pwa','private-browser-control']
    entries=result['migrationEntries'] if migrated else result['freshEntries']
    normal_expected='qa-floorp-custom' if case['case']=='custom-browser-remote-pwa' else 'floorp'
    stages=[]
    for observation in case['observations']:
        checks=[];order=case['order'][:observation['afterStep']+1]
        known_pwa_classes={entry['startupWMClass'] for entry in entries.values()}
        markers={tag['marker'] for tag in observation.get('ordinaryTitleMarkers',[])}
        if markers:
            ordinary=[w for w in observation['kwinWindows'] if w['caption'] in markers]
        else:
            # Older retained observations had no title markers. Their real
            # root windowclass distinguishes the normal restored-content window
            # from the known PWA frames in this already-class-fixed product.
            ordinary_titles={w['title'] for w in observation['product']['windows'] if w.get('windowClass') not in known_pwa_classes}
            ordinary=[w for w in observation['kwinWindows'] if w['caption'] in ordinary_titles]
        expected_count=sum(app in ['browser','custom-browser','private-browser'] for app in order)
        if expected_count:
            checks.append({'role':'ordinary-browser','expectedClass':normal_expected,'expectedCount':expected_count,
                'observedClasses':[w['resourceClass'] for w in ordinary],
                'observedDesktopFiles':[w['desktopFileName'] for w in ordinary],
                'matches':len(ordinary)==expected_count and all(w['resourceClass']==normal_expected and w['desktopFileName']==normal_expected for w in ordinary)})
        for app in ['alpha','beta']:
            if app not in order:continue
            name=entries[app]['app']['name'];expected=entries[app]['startupWMClass']
            actual=[w for w in observation['kwinWindows'] if w['caption'].startswith(name+' —')]
            checks.append({'role':app,'expectedClass':expected,'observedClasses':[w['resourceClass'] for w in actual],
                'observedDesktopFiles':[w['desktopFileName'] for w in actual],
                'matches':len(actual)==1 and actual[0]['resourceClass']==expected and actual[0]['desktopFileName']==expected})
        if 'private-browser' in order:
            private_windows=[w for w in observation['product']['windows'] if w.get('isPrivate') is True]
            checks.append({'role':'actual-private-window','observedCount':len(private_windows),'matches':len(private_windows)==1 and not private_windows[0].get('taskbarId') and not private_windows[0].get('ssbArgumentId')})
        stages.append({'afterStep':observation['afterStep'],'checks':checks,
            'allMatch':all(check['matches'] for check in checks),
            'sandbox':observation['product']['sandbox'],'experiment':observation['product']['experiment']})
    windows={}
    for event in events:
        if event['log']!=case['case']+'-0.log':continue
        match=re.search(r'xdg_toplevel#(\d+)\.(set_app_id|set_title)\((".*")\)',event['event'])
        if not match:continue
        wid,method,value=match.groups()
        try:value=json.loads(value)
        except json.JSONDecodeError:value=value[1:-1]
        windows.setdefault(wid,{'toplevel':wid})[method]=value
    rows.append({'case':case['case'],'order':case['order'],'completed':case.get('completed'),
        'stages':stages,'finalAllMatch':stages[-1]['allMatch'],'protocolFinalWindows':list(windows.values()),'quit':case.get('quit')})
out={'origin':result['origin'],'completed':result['completed'],'cases':rows,
    'productUnchanged':result['productUnchanged'],'originalMigrationFixture':result['migrationFixture'],
    'remainingOwnedProcesses':result['remainingOwnedProcesses'],'runtimeRemoved':result.get('runtimeRemoved')}
(args.result.parent/'identity-projection.json').write_text(json.dumps(out,indent=2,ensure_ascii=False))
print(json.dumps({'completed':out['completed'],'cases':[{'case':row['case'],'matches':row['finalAllMatch']} for row in rows]},ensure_ascii=False))
