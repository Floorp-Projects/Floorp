#!/usr/bin/env python3
"""Read-only monitor for the single already-dispatched Linux QA run."""
import json
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parent
BASE = 'repos/Floorp-Projects/Floorp/actions/runs/42123754997'

def api(path):
    return json.loads(subprocess.check_output(['gh', 'api', path]))

history = []
previous = None
while True:
    run = api(BASE)
    safe = {k: run.get(k) for k in ['id', 'head_sha', 'status', 'conclusion',
            'event', 'path', 'display_title', 'html_url', 'run_attempt',
            'created_at', 'updated_at']}
    assert safe['head_sha'] == '214ba6657a63314594de3867785441558b7a63b5'
    jobs = api(BASE + '/jobs')
    safe_jobs = [{k: job.get(k) for k in ['id', 'name', 'status', 'conclusion',
                  'steps', 'started_at', 'completed_at']} for job in jobs['jobs']]
    active = [s['name'] for j in safe_jobs for s in j['steps']
              if s['status'] == 'in_progress']
    signature = (safe['status'], safe['conclusion'], active)
    if signature != previous:
        state = {'status': safe['status'], 'conclusion': safe['conclusion'],
                 'active': active}
        history.append(state)
        print(json.dumps(state), flush=True)
        (ROOT / 'progress-history.json').write_text(json.dumps(history, indent=2)+'\n')
        previous = signature
    (ROOT / 'package-run-api.json').write_text(json.dumps(safe, indent=2)+'\n')
    (ROOT / 'package-jobs-api.json').write_text(json.dumps(safe_jobs, indent=2)+'\n')
    if safe['status'] == 'completed':
        artifacts = api(BASE + '/artifacts')
        selected = [{k: a.get(k) for k in ['id', 'name', 'size_in_bytes', 'digest',
                     'expired', 'created_at', 'expires_at', 'workflow_run']}
                    for a in artifacts['artifacts']]
        (ROOT / 'package-artifacts-api.json').write_text(json.dumps(selected, indent=2)+'\n')
        raise SystemExit(0 if safe['conclusion'] == 'success' else 1)
    time.sleep(30)
