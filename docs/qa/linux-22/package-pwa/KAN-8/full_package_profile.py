#!/usr/bin/env python3
"""Full locally built PR198 Flatpak profile test with normal session bus/quit.

The candidate launcher/helper are installed from its full manifest build; no helper is injected.

No sandbox level, signature policy or experiment setting is changed.
The host lacks a full system bus; DBUS_SYSTEM_BUS_ADDRESS points to a dedicated
test bus with no Accounts service. This does not qualify a full Bazzite desktop.
"""
import base64
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import time

BASE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BASE))
from pwa_ui_probe import Marionette

OUT = Path(__file__).resolve().parent
SYSROOT = BASE / 'sysroot'
FLATPAK = SYSROOT / 'usr/bin/flatpak'
HELPER = OUT / 'flathub/src/floorp-profile-root.sh'
PORT = 2832

def run_case(case, candidate):
    home = OUT / (case + '-home')
    home.mkdir(exist_ok=False)
    appdata = home / '.var/app/one.ablaze.floorp'
    (appdata / 'config').mkdir(parents=True)
    if candidate:
        (appdata / 'config/qa-helper.sh').write_bytes(HELPER.read_bytes())
    env = dict(os.environ, HOME=str(home),
        XDG_DATA_HOME=str(OUT / 'full-build-home/.local/share'),
        XDG_CONFIG_HOME=str(home / '.config'), XDG_CACHE_HOME=str(home / '.cache'),
        XDG_RUNTIME_DIR=str(BASE / 'runtime'), DISPLAY=':89',
        DBUS_SYSTEM_BUS_ADDRESS='unix:path=' + str(OUT / 'system-probe-bus'),
        FLATPAK_BWRAP='/usr/bin/bwrap',
        FLATPAK_DBUSPROXY=str(SYSROOT / 'usr/bin/xdg-dbus-proxy'),
        LD_LIBRARY_PATH=str(SYSROOT / 'usr/lib/x86_64-linux-gnu'),
        PATH=str(SYSROOT / 'usr/bin') + ':/usr/bin:/bin')
    env.pop('MOZ_DISABLE_CONTENT_SANDBOX', None)
    rows = []
    for label, expose_home in [('initial', False), ('home-enabled', True), ('home-disabled', False)]:
        command = ['dbus-run-session', '--', str(FLATPAK), 'run', '--user',
            '--env=MOZ_REMOTE_ALLOW_SYSTEM_ACCESS=1',
            '--env=MOZ_MARIONETTE_PREF_STATE_ACROSS_RESTARTS=' + json.dumps({'marionette.port':PORT})]
        if expose_home:
            command += ['--filesystem=home']
        if candidate:
            command += ['--command=bash', 'one.ablaze.floorp', '-c',
                'source "$XDG_CONFIG_HOME/qa-helper.sh"; floorp_select_profile_root || exit 1; '
                'export TMPDIR="$XDG_CACHE_HOME/tmp"; '
                'exec /app/lib/floorp/floorp --name one.ablaze.floorp --marionette about:blank']
        else:
            command += ['one.ablaze.floorp', '--marionette', 'about:blank']
        with (OUT / f'{case}-{label}.log').open('w') as log:
            process = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT)
            browser = None
            try:
                deadline = time.monotonic() + 55
                while time.monotonic() < deadline:
                    if process.poll() is not None:
                        raise RuntimeError(f'product exited early: {process.returncode}')
                    try:
                        probe = socket.create_connection(('127.0.0.1', PORT), timeout=.2)
                        probe.close()
                        browser = Marionette('127.0.0.1', PORT)
                        break
                    except (ConnectionRefusedError, socket.timeout):
                        time.sleep(.2)
                if browser is None:
                    raise TimeoutError('Marionette did not become ready')
                if label == 'initial':
                    browser.script('Services.prefs.setStringPref("qa.kan8.marker", "keep-this-profile"); return true;')
                row = browser.script('''
                    const dir = key => Services.dirsvc.get(key, Ci.nsIFile).path;
                    return {profile:dir('ProfD'), localProfile:dir('ProfLD'),
                        marker:Services.prefs.getStringPref('qa.kan8.marker','MISSING'),
                        sandboxLevel:Services.prefs.getIntPref('security.sandbox.content.level'),
                        version:Services.appinfo.version, buildID:Services.appinfo.appBuildID,
                        appID:Services.appinfo.ID, appName:Services.appinfo.name,
                        sandboxDisableEnv:Services.env.exists('MOZ_DISABLE_CONTENT_SANDBOX')};
                ''')
                row['label'] = label
                row['homePermission'] = expose_home
                try:
                    image = browser.call('WebDriver:TakeScreenshot', {'full':True})
                    data = image.get('value', image)
                    (OUT / f'{case}-{label}.png').write_bytes(base64.b64decode(data))
                except Exception as error:
                    row['screenshotError'] = str(error)
                try:
                    row['quit'] = browser.call('Marionette:Quit', {'flags':['eAttemptQuit']})
                finally:
                    browser.socket.close()
                row['exitCode'] = process.wait(timeout=35)
                row['normalExit'] = row['exitCode'] == 0
                rows.append(row)
                (OUT / f'{case}-result.json').write_text(json.dumps(rows, indent=2))
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=8)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
    stable = len({row['profile'] for row in rows}) == 1
    return {'case':case, 'helperInjected':candidate, 'fullCandidatePackage':True, 'rows':rows,
        'stableProfile':stable, 'allMarkers':all(row['marker']=='keep-this-profile' for row in rows),
        'allNormalExits':all(row['normalExit'] for row in rows)}

if __name__ == '__main__':
    result = []
    try:
        result.append(run_case('full-package-normal', False))
    finally:
        (OUT / 'full-package-normal-result.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))
