#!/usr/bin/env python3
"""Observe real Wayland identities across process-first orders and an old QA profile.

No identity is assigned or mocked by this harness. Products create their own
launchers via actual UI; migrated launcher executable/profile paths are relocated
as arguments only, preserving --name, --start-ssb and persisted SSB identities.
"""
import argparse
import configparser
import hashlib
import http.server
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import socket
import subprocess
import sys
import threading
import time
import tempfile
from urllib.parse import urlsplit

QA_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(QA_ROOT))
sys.path.insert(0, str(QA_ROOT / 'full-build'))
from pwa_ui_probe import Fixture, Marionette
from full_package_pwa_probe import inventory, wait_client, release, quit_owned, PRODUCT_SNAPSHOT

OBSERVATION_SNAPSHOT = PRODUCT_SNAPSHOT.replace(
    'windowClass:w.document.documentElement.getAttribute(\'windowclass\'),',
    "taskbarId:w.document.documentElement.getAttribute('taskbartab')," +
    "ssbArgumentId:(()=>{try{return w.arguments?.[1]?.getPropertyAsAString('ssbid')??null;}catch{return null;}})()," +
    "originalTestTitle:w.__QA_WBID_ORIGINAL_TITLE__??null," +
    "isPrivate:ChromeUtils.importESModule('resource://gre/modules/PrivateBrowsingUtils.sys.mjs').PrivateBrowsingUtils.isWindowPrivate(w)," +
    "windowClass:w.document.documentElement.getAttribute('windowclass'),",
)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read_entries(home, profile, product, relocated=False):
    store = json.loads((profile / 'ssb/ssb.json').read_text())
    entries = {}
    for app in store.values():
        if app['name'] not in ['QA PWA Alpha', 'QA PWA Beta']:
            continue
        label = 'alpha' if app['name'].endswith('Alpha') else 'beta'
        desktop = home / '.local/share/applications' / ('floorp-' + app['id'].strip('{}').lower() + '.desktop')
        config = configparser.ConfigParser(interpolation=None); config.read(desktop)
        values = config['Desktop Entry']; command = shlex.split(values['Exec'])
        original = command.copy()
        assert command[command.index('--name') + 1] == values['StartupWMClass']
        assert command[command.index('--start-ssb') + 1] == app['id']
        if relocated:
            command[0] = str(product / 'floorp')
            command[command.index('--profile') + 1] = str(profile)
        else:
            assert Path(command[0]).resolve() == (product / 'floorp').resolve()
            assert command[command.index('--profile') + 1] == str(profile)
        entries[label] = {'app': {key: app.get(key) for key in ['id', 'name', 'start_url', 'userContextId']},
            'originalExec': original, 'effectiveExec': command, 'startupWMClass': values['StartupWMClass'],
            'desktopSha256': digest(desktop), 'relocatedPathsOnly': relocated}
    assert set(entries) == {'alpha', 'beta'}
    return entries


def set_automation_port(profile, port):
    path = profile / 'user.js'
    text = path.read_text() if path.exists() else ''
    text = re.sub(r'user_pref\("marionette\.port",\s*\d+\);\s*', '', text)
    path.write_text(text + f'\nuser_pref("marionette.port", {port});\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--intake', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--origin', choices=['authenticated-full-package', 'patched-product-copy'], default='authenticated-full-package')
    parser.add_argument('--product', type=Path)
    parser.add_argument('--candidate-provenance', type=Path)
    parser.add_argument('--sysroot', type=Path, default=QA_ROOT / 'sysroot')
    parser.add_argument('--migration-profile', type=Path, default=QA_ROOT / 'full-build/3032/wayland-product-qa/profile')
    parser.add_argument('--migration-home', type=Path, default=QA_ROOT / 'full-build/3032/wayland-product-qa/home')
    parser.add_argument('--display', default=':96')
    parser.add_argument('--port', type=int, default=2880)
    parser.add_argument('--fixture-port', type=int, default=8790)
    parser.add_argument('--custom-control-only', action='store_true')
    parser.add_argument('--private-control-only', action='store_true')
    parser.add_argument('--diagnostic-ui-only', action='store_true')
    parser.add_argument('--trace-product-fileops', action='store_true')
    args = parser.parse_args()
    assert not (args.custom_control_only and args.private_control_only)
    meta = json.loads(args.intake.read_text()); run, artifact = meta['run'], meta['artifact']
    assert run['status'] == 'completed' and run['conclusion'] == 'success'
    assert meta['sourceSha'] == run['head_sha'] == artifact['workflow_run']['head_sha']
    assert artifact['workflow_run']['id'] == run['id'] and artifact['expired'] is False
    assert artifact['digest'] == 'sha256:' + meta['archiveZipSha256']
    base_product = Path(meta['productRoot']).resolve(strict=True)
    base_inventory = inventory(base_product)
    assert base_inventory == json.loads((args.intake.parent / meta['productInventoryFile']).read_text())
    product = (args.product or base_product).resolve(strict=True)
    before = inventory(product)
    origin = {'kind': args.origin, 'baseSourceSha': meta['sourceSha'], 'baseRunId': run['id'],
        'baseArtifactId': artifact['id'], 'baseArtifactDigest': artifact['digest'], 'productRoot': str(product)}
    if args.origin == 'authenticated-full-package':
        assert product == base_product and before == base_inventory
    else:
        assert product != base_product and args.candidate_provenance
        origin['candidateProvenance'] = json.loads(args.candidate_provenance.read_text())
        origin['candidateProvenanceSha256'] = digest(args.candidate_provenance)
        old = {row['path']: row for row in base_inventory}
        origin['changedProductEntries'] = [row for row in before if row != old.get(row['path'])]
        assert origin['changedProductEntries'], 'Patched-copy label requires actual changed files'
    assert not args.out.exists(), 'Refusing to reuse an evidence/profile root'
    assert re.fullmatch(r':[0-9]+', args.display)
    assert not Path('/tmp/.X11-unix/X' + args.display[1:]).exists(), 'Display already owned'
    assert os.environ.get('DBUS_SESSION_BUS_ADDRESS'), 'Run in a dedicated dbus-run-session'
    assert not os.environ.get('MOZ_DISABLE_CONTENT_SANDBOX'), 'Disabled sandbox forbidden'
    for port in [args.port, args.port + 1]:
        with socket.socket() as probe:
            assert probe.connect_ex(('127.0.0.1', port)) != 0, 'Automation port in use'
    args.out.mkdir(parents=True)
    home, profile = [args.out / name for name in ['home', 'profile']]
    # AF_UNIX includes the complete XDG_RUNTIME_DIR/socket name in its 108-byte
    # limit. Keep this normal private runtime short; evidence/profile paths stay
    # in the dedicated workspace area.
    runtime = Path(tempfile.mkdtemp(prefix='wbid-runtime-', dir='/tmp'))
    for path in [home, profile]: path.mkdir(mode=0o700)
    set_automation_port(profile, args.port)
    source_profile_before = inventory(args.migration_profile)
    source_home_before = inventory(args.migration_home)
    original_store_sha = digest(args.migration_profile / 'ssb/ssb.json')
    migration_profile, migration_home = args.out / 'migration-profile', args.out / 'migration-home'
    shutil.copytree(args.migration_profile, migration_profile, symlinks=True,
        ignore=shutil.ignore_patterns('lock', 'parent.lock', '.parentlock'))
    shutil.copytree(args.migration_home, migration_home, symlinks=True)
    assert digest(migration_profile / 'ssb/ssb.json') == original_store_sha
    set_automation_port(migration_profile, args.port + 1)
    result = {'origin': origin, 'runtimeDir': str(runtime), 'customControlOnly': args.custom_control_only,
        'privateControlOnly': args.private_control_only,
        'diagnosticOnly': args.diagnostic_ui_only or args.trace_product_fileops,
        'scope': 'actual native Wayland protocol/KWin/desktop identities; no mock assignments',
        'input': 'synthetic UI installation and generated-launcher CLI startup; no physical-input claim',
        'migrationFixture': {'sourceProfile': str(args.migration_profile), 'sourceHome': str(args.migration_home),
            'sourceSsbStoreSha256': original_store_sha, 'excludedTransientLocks': ['lock', 'parent.lock', '.parentlock'],
            'changesBeforeRun': 'automation port only; no SSB/identity/desktop edits'}, 'orders': [], 'completed': False}
    env = dict(os.environ, HOME=str(home), XDG_DATA_HOME=str(home / '.local/share'),
        XDG_CONFIG_HOME=str(home / '.config'), XDG_CACHE_HOME=str(home / '.cache'),
        XDG_RUNTIME_DIR=str(runtime), DISPLAY=args.display, MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',
        MOZ_ENABLE_WAYLAND='1', GDK_BACKEND='wayland', WAYLAND_DEBUG='1',
        LD_LIBRARY_PATH=str(args.sysroot / 'usr/lib/x86_64-linux-gnu'),
        XDG_DATA_DIRS=str(args.sysroot / 'usr/share') + ':' + str(migration_home / '.local/share') + ':/usr/share',
        XDG_CONFIG_DIRS=str(args.sysroot / 'etc/xdg'),
        QT_PLUGIN_PATH=str(args.sysroot / 'usr/lib/x86_64-linux-gnu/qt6/plugins'),
        QML2_IMPORT_PATH=str(args.sysroot / 'usr/lib/x86_64-linux-gnu/qt6/qml'))
    owned, logs, servers = [], [], []
    xvfb = kwin = browser = client = None
    def launch(command, label, launch_env):
        log = (args.out / (label + '.log')).open('wb'); logs.append(log)
        if args.trace_product_fileops and label == 'ui-browser':
            # Main process only: do not attach to Gecko sandboxed content
            # children. Diagnostic trace is never qualification evidence.
            command = ['strace', '-k', '-ttt', '-s', '256', '-e',
                'trace=openat,openat2,symlink,symlinkat,unlink,unlinkat,chdir,fchdir',
                '-P', str(product / '.parentlock'), '-P', str(product / 'lock'),
                '-o', str(args.out / 'profile-lock.strace'), '--', *command]
        process = subprocess.Popen(command, env=launch_env, stdout=log, stderr=subprocess.STDOUT)
        owned.append(process); return process
    def observe(row, step, selected_profile, selected_port):
        nonlocal client
        # Only tag ordinary window *titles* so session-restored content titles
        # cannot be confused with PWA windows. Native identity is never assigned
        # by the harness. Root approved this presentation-only marker control.
        tags = client.script(r"""
const tags=[];
for(const win of Services.wm.getEnumerator('navigator:browser')){
 let ssb=null;try{ssb=win.arguments?.[1]?.getPropertyAsAString('ssbid')??null;}catch{}
 const taskbar=win.document.documentElement.getAttribute('taskbartab');
 if(ssb||taskbar)continue;
 if(!('__QA_WBID_ORIGINAL_TITLE__' in win))win.__QA_WBID_ORIGINAL_TITLE__=win.document.title;
 if(!('__QA_WBID_TITLE_MARKER__' in win))win.__QA_WBID_TITLE_MARKER__='QA_TEST_BROWSER_'+Services.appinfo.processID+'_'+Services.uuid.generateUUID().toString().replace(/[{}-]/g,'');
 const marker=win.__QA_WBID_TITLE_MARKER__;
 win.document.title=marker;
 tags.push({marker,originalTitle:win.__QA_WBID_ORIGINAL_TITLE__,url:win.gBrowser?.selectedBrowser?.currentURI?.spec});
}
return tags;
""")
        time.sleep(.3)
        data = client.script(OBSERVATION_SNAPSHOT)
        assert Path(data['profile']).resolve() == selected_profile.resolve()
        assert data['sandbox'] == 6 and data['experiment'] is False
        marker = 'QA_WBID_' + str(os.getpid()) + '_' + str(len(result['orders'])) + '_' + str(step)
        script = args.out / (marker + '.js')
        script.write_text('print(' + json.dumps(marker + '=') +
            '+JSON.stringify(workspace.windowList().map(w=>({caption:w.caption,resourceClass:w.resourceClass,'
            'desktopFileName:w.desktopFileName,resourceName:w.resourceName,pid:w.pid}))));\n')
        qdbus = str(args.sysroot / 'usr/bin/qdbus6'); name = marker.lower()
        script_id = subprocess.check_output([qdbus, 'org.kde.KWin', '/Scripting',
            'org.kde.kwin.Scripting.loadScript', str(script), name], env=env, text=True, timeout=15).strip()
        try:
            subprocess.run([qdbus, 'org.kde.KWin', '/Scripting/Script' + script_id,
                'org.kde.kwin.Script.run'], env=env, check=True, capture_output=True, timeout=15)
        finally:
            subprocess.run([qdbus, 'org.kde.KWin', '/Scripting',
                'org.kde.kwin.Scripting.unloadScript', name], env=env, check=True, capture_output=True, timeout=15)
        deadline = time.monotonic() + 5; lines = []
        while not lines and time.monotonic() < deadline:
            lines = [line for line in (args.out / 'kwin.log').read_text().splitlines() if marker + '=' in line]
            if not lines: time.sleep(.1)
        row.setdefault('observations', []).append({'afterStep': step, 'product': data, 'ordinaryTitleMarkers': tags,
            'kwinWindows': [w for w in json.loads(lines[-1].split(marker + '=', 1)[1]) if w['pid'] == data['pid']]})
    try:
        xvfb = launch([str(args.sysroot / 'usr/bin/Xvfb'), args.display,
            '-screen', '0', '1280x900x24', '-nolisten', 'tcp'], 'xvfb', env)
        deadline = time.monotonic() + 10
        while not Path('/tmp/.X11-unix/X' + args.display[1:]).exists():
            if xvfb.poll() is not None or time.monotonic() > deadline: raise RuntimeError('Owned Xvfb unavailable')
            time.sleep(.1)
        socket_name = 'wbid-' + str(os.getpid())
        kde_env = dict(env, QT_QPA_PLATFORM='xcb'); kde_env.pop('WAYLAND_DEBUG', None); kde_env.pop('WAYLAND_DISPLAY', None)
        kwin = launch([str(args.sysroot / 'usr/bin/kwin_wayland'), '--x11-display', args.display,
            '--socket', socket_name, '--no-lockscreen', '--width', '1000', '--height', '720'], 'kwin', kde_env)
        deadline = time.monotonic() + 20
        while not (runtime / socket_name).exists():
            if kwin.poll() is not None or time.monotonic() > deadline: raise RuntimeError('Owned Wayland socket unavailable')
            time.sleep(.1)
        env.update(WAYLAND_DISPLAY=socket_name, QT_QPA_PLATFORM='wayland')
        while True:
            ready = subprocess.run([str(args.sysroot / 'usr/bin/qdbus6'), 'org.freedesktop.DBus', '/org/freedesktop/DBus',
                'org.freedesktop.DBus.NameHasOwner', 'org.kde.KWin'], env=env, text=True, capture_output=True, timeout=5)
            if ready.returncode == 0 and ready.stdout.strip() == 'true': break
            if kwin.poll() is not None or time.monotonic() > deadline: raise RuntimeError('Owned KWin session service unavailable')
            time.sleep(.2)
        fresh_entries = {}
        if not (args.custom_control_only or args.private_control_only):
            browser = launch([str(product / 'floorp'), '--no-remote', '--profile', str(profile), '--marionette', 'about:blank'], 'ui-browser', env)
            client = wait_client(args.port, profile, browser); release(client); client = None
            ui = subprocess.run([sys.executable, str(QA_ROOT / 'pwa_ui_probe.py'), '--port', str(args.port),
                '--fixture-port', str(args.fixture_port), '--out', str(args.out / 'ui-create')], env=env,
                capture_output=True, text=True, timeout=180)
            (args.out / 'ui-probe.log').write_text(ui.stdout + ui.stderr)
            result['ui'] = json.loads((args.out / 'ui-create/result.json').read_text())
            assert ui.returncode == 0 and result['ui']['completed'], 'Actual UI fixture creation failed'
            client = wait_client(args.port, profile, browser)
            result['uiQuit'] = quit_owned(client, browser); client = browser = None
            fresh_entries = read_entries(home, profile, product)
        migrated_entries = read_entries(migration_home, migration_profile, product, relocated=True)
        result['freshEntries'] = fresh_entries; result['migrationEntries'] = migrated_entries
        ports = set() if (args.custom_control_only or args.private_control_only) else {args.fixture_port}
        ports.update(urlsplit(entry['app']['start_url']).port for entry in migrated_entries.values())
        for port in ports:
            server = http.server.ThreadingHTTPServer(('127.0.0.1', port), Fixture)
            threading.Thread(target=server.serve_forever, daemon=True).start(); servers.append(server)
        cases = [('browser-pwa', ['browser', 'alpha'], profile, home, args.port, fresh_entries),
            ('pwa-browser', ['alpha', 'browser'], profile, home, args.port, fresh_entries),
            ('multiple-pwa', ['alpha', 'beta', 'browser'], profile, home, args.port, fresh_entries),
            ('existing-profile-migration', ['alpha', 'browser', 'beta'], migration_profile, migration_home, args.port + 1, migrated_entries)]
        if args.custom_control_only:
            cases = [('custom-browser-remote-pwa', ['custom-browser', 'alpha', 'browser'],
                migration_profile, migration_home, args.port + 1, migrated_entries)]
        if args.private_control_only:
            cases = [('private-browser-control', ['alpha', 'private-browser'],
                migration_profile, migration_home, args.port + 1, migrated_entries)]
        if args.diagnostic_ui_only:
            cases = []
        for label, order, case_profile, case_home, port, entries in cases:
            row = {'case': label, 'order': order, 'profile': str(case_profile), 'commands': []}
            result['orders'].append(row)
            case_env = dict(env, HOME=str(case_home), XDG_DATA_HOME=str(case_home / '.local/share'),
                XDG_CONFIG_HOME=str(case_home / '.config'), XDG_CACHE_HOME=str(case_home / '.cache'))
            for step, application in enumerate(order):
                command = ([str(product / 'floorp'), '--profile', str(case_profile), 'about:blank'] if application in ['browser', 'custom-browser', 'private-browser']
                    else entries[application]['effectiveExec'].copy())
                if application == 'custom-browser': command[1:1] = ['--name', 'qa-floorp-custom']
                if application == 'browser' and step > 0: command[-1:-1] = ['--new-window']
                if application == 'private-browser': command[-1:-1] = ['--private-window']
                if step == 0: command.append('--marionette')
                row['commands'].append(command)
                process = launch(command, label + '-' + str(step), case_env)
                if step == 0:
                    browser = process; client = wait_client(port, case_profile, browser)
                time.sleep(4)
                observe(row, step, case_profile, port)
            row['quit'] = quit_owned(client, browser); client = browser = None
            row['completed'] = True
            (args.out / 'result.json').write_text(json.dumps(result, indent=2))
        result['completed'] = True
    except Exception as error:
        result['error'] = str(error)
    finally:
        if client:
            try:
                if browser: result['cleanupQuit'] = quit_owned(client, browser)
                else: release(client)
            except Exception as error: result['cleanupError'] = str(error)
        for server in servers: server.shutdown(); server.server_close()
        for process in reversed(owned):
            if process.poll() is None:
                # Display/compositor are always owned; product fallback is explicit.
                if process not in [xvfb, kwin]: result.setdefault('ownedProductTerminationFallback', []).append(process.pid)
                process.terminate(); process.wait(timeout=15)
        for log in logs: log.close()
        result['productUnchanged'] = inventory(product) == before
        result['baseProductUnchanged'] = inventory(base_product) == base_inventory
        result['migrationFixture']['originalProfileUnchanged'] = inventory(args.migration_profile) == source_profile_before
        result['migrationFixture']['originalHomeUnchanged'] = inventory(args.migration_home) == source_home_before
        result['migrationFixture']['originalStoreUnchanged'] = digest(args.migration_profile / 'ssb/ssb.json') == original_store_sha
        result['remainingOwnedProcesses'] = [p.pid for p in owned if p.poll() is None]
        shutil.rmtree(runtime)
        result['runtimeRemoved'] = not runtime.exists()
        protocol = []
        for path in args.out.glob('*.log'):
            if path.name in ['kwin.log', 'xvfb.log']: continue
            protocol.extend({'log': path.name, 'event': line} for line in path.read_text(errors='replace').splitlines()
                if 'xdg_toplevel' in line and ('set_app_id' in line or 'set_title' in line))
        result['nativeWaylandProtocolObserved'] = any('set_app_id' in row['event'] for row in protocol)
        (args.out / 'native-protocol.json').write_text(json.dumps(protocol, indent=2))
        if not all([result['productUnchanged'], result['baseProductUnchanged'],
            result['migrationFixture']['originalProfileUnchanged'], result['migrationFixture']['originalHomeUnchanged'],
            result['nativeWaylandProtocolObserved']]): result['completed'] = False
        (args.out / 'result.json').write_text(json.dumps(result, indent=2))
    print(json.dumps({key: result.get(key) for key in ['completed', 'error', 'origin', 'productUnchanged', 'remainingOwnedProcesses']}))
    return 0 if result['completed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
