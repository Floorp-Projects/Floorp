#!/usr/bin/env python3
"""Validate completed actual-product records and emit their compact matrix."""
import configparser
import json
from pathlib import Path

root = Path(__file__).resolve().parent.parent / 'full-product'
labels = ['baseline', 'candidate', 'baseline-reverse', 'candidate-reverse', 'api-baseline', 'api-candidate']
runs = {name: json.loads((root / name / 'result.json').read_text()) for name in labels}
assert all(r['completed'] and r['productUnchanged'] and r['ownedProcessesRemaining'] == 0 for r in runs.values())
assert all(runs[n]['allExpectedChecksPass'] for n in labels if n != 'api-baseline')
assert len(runs['api-baseline']['checks']) == 2 and not any(runs['api-baseline']['checks'].values())
identities = []
for name in labels:
    for file in (root / name).glob('*startup-before-hub.json'):
        record = json.loads(file.read_text())
        assert record['version'] == '159.0.1' and record['buildID'] == '20261103153232'
        assert record['sandbox'] == 6 and not record['sandboxDisabled']
        identities.append({'file': str(file.relative_to(root)), 'version': record['version'],
                           'buildID': record['buildID'], 'sandbox': record['sandbox']})
ini = {}
for name in ['baseline', 'candidate']:
    ini[name] = {}
    for file, source in json.loads((root / name / 'product-ini-metadata.json').read_text()).items():
        parser = configparser.ConfigParser(interpolation=None)
        parser.read_string(source)
        ini[name][file] = {section: dict(parser[section]) for section in parser.sections()}

conditions = [
    {'condition': 'cold nativefalse/storedvertical, all tabs blank before Hub', 'baseline': 'reproduced, persists normal restart', 'candidate': 'pass false/horizontal', 'evidence': ['baseline/08-baseline-restart-startup-before-hub.json', 'candidate/01-candidate-startup-before-hub.json']},
    {'condition': 'cold nativetrue/storedhorizontal, all tabs blank before Hub', 'baseline': 'reproduced, persists normal restart', 'candidate': 'pass true/vertical', 'evidence': ['baseline-reverse/08-baseline-restart-startup-before-hub.json', 'candidate-reverse/01-candidate-startup-before-hub.json']},
    {'condition': 'Hub Vertical -> actual native/rendered vertical', 'baseline': 'pass', 'candidate': 'pass both migrated profiles', 'evidence': ['baseline/03-hub-to-native-render.json', 'candidate/03-hub-to-native-render.json', 'candidate-reverse/03-hub-to-native-render.json']},
    {'condition': 'native Horizontal -> Hub/persisted horizontal', 'baseline': 'reproduced stale vertical', 'candidate': 'pass both profiles', 'evidence': ['baseline/05-hub-after-off.json', 'candidate/05-hub-after-off.json', 'candidate-reverse/05-hub-after-off.json']},
    {'condition': 'native Vertical -> Hub/persisted vertical', 'baseline': 'reproduced stale horizontal', 'candidate': 'pass both profiles', 'evidence': ['baseline-reverse/05-hub-after-on.json', 'candidate/07-hub-after-on.json', 'candidate-reverse/07-hub-after-on.json']},
    {'condition': 'real settled New Window', 'baseline': 'not repeated in new complete-product runs', 'candidate': 'pass both profiles, two ready windows actually vertical', 'evidence': ['candidate/08-new-window-ready.json', 'candidate-reverse/08-new-window-ready.json']},
    {'condition': 'actual Workspace first/second/first and multirow', 'baseline': 'not repeated in new complete-product runs', 'candidate': 'pass both profiles; few tabs only', 'evidence': ['candidate/11-workspace-first.json', 'candidate/12-workspace-second.json', 'candidate/13-workspace-return.json', 'candidate/14-multirow-after-workspace.json', 'candidate-reverse/14-multirow-after-workspace.json']},
    {'condition': 'multirow through normal restart', 'baseline': 'not repeated in new complete-product runs', 'candidate': 'pass both profiles', 'evidence': ['candidate/16-normal-restart-startup-before-hub.json', 'candidate/17-multirow-after-restart.json', 'candidate-reverse/16-normal-restart-startup-before-hub.json', 'candidate-reverse/17-multirow-after-restart.json']},
    {'condition': 'shipped API stale horizontal/latest multirow, unrelated favicon save', 'baseline': 'fail, wrongly persists horizontal', 'candidate': 'pass, latest multirow and favicon edit persist/nativefalse', 'evidence': ['api-baseline/shipped-api-cases.json', 'api-candidate/shipped-api-cases.json']},
    {'condition': 'shipped API stale multirow/latest horizontal, unrelated favicon save', 'baseline': 'fail, wrongly persists multirow', 'candidate': 'pass, latest horizontal and favicon edit persist/nativefalse', 'evidence': ['api-baseline/shipped-api-cases.json', 'api-candidate/shipped-api-cases.json']},
]
result = {
    'key': 'KAN-24', 'scope': 'additional target, originalLinux22 denominator unchanged', 'originalLinuxDenominator': 22,
    'products': json.loads((root.parent / 'full-product-intake-summary.json').read_text()),
    'liveIdentity': identities, 'separatePackageIniMetadata': ini, 'conditions': conditions,
    'executionRuns': [{'label': label, **{k: v for k, v in runs[label].items() if k != 'records'}} for label in labels],
    'normalQuits': sum(len(r['normalQuits']) for r in runs.values()),
    'retainedHarnessAttempt': 'baseline-attempt1-navigation/result.json',
    'remainingConditions': ['Original macOS Photon not accessed; wait for KAN10 owner.', 'Two-visible-window pending autosave race not established; shipped API uses real external pref writer.', 'Few-tab multirow persistence only, not overflowing geometry.', 'Complete package uses locked prebuilt native Runtime; not fresh Gecko compilation or strict release qualification.'],
    'visualInspection': ['baseline/03-hub-to-native-hub.png', 'baseline-reverse/05-hub-after-on.png', 'candidate/08-new-window-ready.png', 'candidate-reverse/02-migration-hub.png'],
}
(root / 'matrix.json').write_text(json.dumps(result, indent=2) + '\n')
assert not Path('/tmp/.X102-lock').exists() and not Path('/tmp/.X11-unix/X102').exists()
(root / 'cleanup.json').write_text(json.dumps({'ownedProcessesRemaining': 0, 'xvfb102LockAndSocketAbsent': True,
    'successfulNormalQuits': result['normalQuits'], 'allProductsUnchanged': True, 'retainedFailedAttemptAlsoNormalQuit': True}, indent=2) + '\n')
print('6 completed runs; both startup directions repaired; shipped API baseline2fail -> candidate2pass; normalquits=' + str(result['normalQuits']))
