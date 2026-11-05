#!/usr/bin/env python3
"""Exercise an authenticated, unmodified full workflow package in fresh QA state.

Requires package-agent product-intake.json and its original complete inventory.
It never patches a product, copies a previous browser profile, or forces rollout.
All UI input is synthetic. Artifact/provenance authentication remains a separate
intake step; its exact source/run/digest and product inventory are checked here.
"""
import argparse
import configparser
import hashlib
import http.server
import json
import os
from pathlib import Path
import re
import select
import shlex
import socket
import subprocess
import sys
import threading
import time

QA_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(QA_ROOT))
from pwa_ui_probe import Fixture, Marionette


def inventory(product):
    rows = []
    for file in sorted(product.rglob('*')):
        if file.is_symlink():
            rows.append({'path': str(file.relative_to(product)), 'symlink': str(file.readlink())})
        elif file.is_file():
            with file.open('rb') as stream:
                digest = hashlib.file_digest(stream, 'sha256').hexdigest()
            rows.append({'path': str(file.relative_to(product)), 'size': file.stat().st_size, 'sha256': digest})
    return rows


def wait_client(port, profile, process):
    deadline = time.monotonic() + 45
    last = None
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f'Owned browser exited before automation: {process.returncode}')
        try:
            client = Marionette('127.0.0.1', port)
            observed = client.script("return Services.dirsvc.get('ProfD',Ci.nsIFile).path;")
            if Path(observed).resolve() != profile.resolve():
                client.socket.close()
                raise RuntimeError('Wrong profile; refusing browser control')
            client.call('WebDriver:SetTimeouts', {'script': 120000})
            return client
        except (ConnectionError, OSError) as error:
            last = str(error)
            time.sleep(.2)
    raise TimeoutError(f'Owned browser automation unavailable: {last}')


def release(client):
    client.call('WebDriver:DeleteSession')
    client.socket.close()


def quit_owned(client, process):
    result = client.call('Marionette:Quit', {'flags': ['eAttemptQuit']})
    client.socket.close()
    process.wait(timeout=20)
    return result


PRODUCT_SNAPSHOT = r"""
return {pid:Services.appinfo.processID,profile:Services.dirsvc.get('ProfD',Ci.nsIFile).path,
 executable:Services.dirsvc.get('XREExeF',Ci.nsIFile).path,version:Services.appinfo.version,
 buildID:Services.appinfo.appBuildID,platformBuildID:Services.appinfo.platformBuildID,sandbox:Services.prefs.getIntPref('security.sandbox.content.level'),
 experiment:ChromeUtils.importESModule('resource://noraneko/modules/pwa/TaskbarExperiment.sys.mjs').TaskbarExperiment.isEnabledForLinux(),
 windows:Array.from(Services.wm.getEnumerator('navigator:browser'),w=>({title:w.document.title,
 windowClass:w.document.documentElement.getAttribute('windowclass'),
 url:w.gBrowser?.selectedBrowser?.currentURI?.spec}))};
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--intake', type=Path, required=True)
    parser.add_argument('--expected-sha', required=True)
    parser.add_argument('--expected-run', type=int, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--sysroot', type=Path, default=QA_ROOT / 'sysroot')
    parser.add_argument('--mode', choices=['x11', 'wayland'], required=True)
    parser.add_argument('--port', type=int, required=True)
    parser.add_argument('--fixture-port', type=int, required=True)
    parser.add_argument('--kwin-log', type=Path)
    parser.add_argument('--display')
    parser.add_argument('--nested-wayland', action='store_true')
    args = parser.parse_args()
    intake = json.loads(args.intake.read_text())
    run, artifact = intake['run'], intake['artifact']
    assert intake['sourceSha'] == args.expected_sha == run['head_sha']
    assert run['id'] == args.expected_run and run['status'] == 'completed' and run['conclusion'] == 'success'
    assert artifact['workflow_run']['id'] == run['id'] and artifact['workflow_run']['head_sha'] == args.expected_sha
    assert artifact['expired'] is False and artifact['name'] == 'noraneko-linux-x86_64-installer-unsigned'
    assert artifact['digest'] == 'sha256:' + intake['archiveZipSha256']
    product = Path(intake['productRoot']).resolve(strict=True)
    binary = product / 'floorp'
    before = inventory(product)
    supplied = json.loads((args.intake.parent / intake['productInventoryFile']).read_text())
    assert before == supplied, 'Product differs from authenticated full-package inventory'
    if args.out.exists():
        raise RuntimeError('Refusing to reuse an output root/profile')
    if os.environ.get('MOZ_DISABLE_CONTENT_SANDBOX'):
        raise RuntimeError('Refusing disabled content sandbox environment')
    with socket.socket() as probe:
        assert probe.connect_ex(('127.0.0.1', args.port)) != 0, 'Automation port already in use'
    args.out.mkdir(parents=True)
    home, profile, runtime = (args.out / name for name in ['home', 'profile', 'runtime'])
    for directory in [home, profile, runtime]:
        directory.mkdir(mode=0o700)
    (profile / 'user.js').write_text(f'user_pref("marionette.port", {args.port});\n')
    env = dict(os.environ, HOME=str(home), XDG_DATA_HOME=str(home / '.local/share'),
        XDG_CONFIG_HOME=str(home / '.config'), XDG_CACHE_HOME=str(home / '.cache'),
        MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',
        LD_LIBRARY_PATH=str(args.sysroot / 'usr/lib/x86_64-linux-gnu'))
    if args.mode == 'wayland':
        if args.nested_wayland:
            env['XDG_RUNTIME_DIR'] = str(runtime)
            args.kwin_log = args.out / 'kwin.log'
        else:
            assert env.get('WAYLAND_DISPLAY') and env.get('XDG_RUNTIME_DIR') and args.kwin_log
        env.update(MOZ_ENABLE_WAYLAND='1', GDK_BACKEND='wayland', WAYLAND_DEBUG='1')
    else:
        env.update(MOZ_ENABLE_WAYLAND='0', GDK_BACKEND='x11', XDG_RUNTIME_DIR=str(runtime))
    result = {'kind': 'authenticated full production application package + prebuilt locked native engine',
        'sourceSha': args.expected_sha, 'runId': run['id'], 'artifactId': artifact['id'],
        'artifactDigest': artifact['digest'], 'packageTarSha256': intake['packageTarSha256'],
        'productRoot': str(product), 'mode': args.mode, 'freshHome': str(home), 'freshProfile': str(profile),
        'input': 'synthetic UI clicks and CLI launch; no physical-input claim', 'completed': False}
    browser = xvfb = kwin = server = client = None
    logs, owned = [], []
    try:
        if args.mode == 'x11' or args.nested_wayland:
            log = (args.out / 'xvfb.log').open('wb'); logs.append(log)
            display_args = []
            read_fd = write_fd = None
            if args.display:
                assert re.fullmatch(r':[0-9]+', args.display), 'Unsupported display selector'
                assert not Path('/tmp/.X11-unix/X' + args.display[1:]).exists(), 'Display already owned'
                display_args = [args.display]
                pass_fds = ()
            else:
                read_fd, write_fd = os.pipe()
                display_args = ['-displayfd', str(write_fd)]
                pass_fds = (write_fd,)
            xvfb = subprocess.Popen([str(args.sysroot / 'usr/bin/Xvfb'), *display_args,
                '-screen', '0', '1280x900x24', '-nolisten', 'tcp'], env=env,
                stdout=log, stderr=subprocess.STDOUT, pass_fds=pass_fds)
            if write_fd is not None:
                os.close(write_fd)
                if not select.select([read_fd], [], [], 10)[0]:
                    raise TimeoutError('Owned Xvfb did not allocate a display')
                display = os.read(read_fd, 32).decode().strip(); os.close(read_fd)
                assert display.isdigit(), 'Unexpected Xvfb display number'
                env['DISPLAY'] = ':' + display
            else:
                env['DISPLAY'] = args.display
                deadline = time.monotonic() + 10
                while not Path('/tmp/.X11-unix/X' + args.display[1:]).exists():
                    if xvfb.poll() is not None or time.monotonic() > deadline:
                        raise RuntimeError('Owned Xvfb failed to become ready')
                    time.sleep(.1)
            result['display'] = env['DISPLAY']
        if args.nested_wayland:
            assert args.mode == 'wayland' and env.get('DBUS_SESSION_BUS_ADDRESS'), 'Run in a dedicated dbus-run-session'
            socket_name = 'qa-fullpackage-pwa-' + str(os.getpid())
            kde_env = dict(env, QT_QPA_PLATFORM='xcb',
                XDG_DATA_DIRS=str(args.sysroot / 'usr/share') + ':/usr/share',
                XDG_CONFIG_DIRS=str(args.sysroot / 'etc/xdg'),
                QT_PLUGIN_PATH=str(args.sysroot / 'usr/lib/x86_64-linux-gnu/qt6/plugins'),
                QML2_IMPORT_PATH=str(args.sysroot / 'usr/lib/x86_64-linux-gnu/qt6/qml'))
            kde_env.pop('WAYLAND_DEBUG', None)
            kde_env.pop('WAYLAND_DISPLAY', None)
            log = args.kwin_log.open('wb'); logs.append(log)
            kwin = subprocess.Popen([str(args.sysroot / 'usr/bin/kwin_wayland'),
                '--x11-display', env['DISPLAY'], '--socket', socket_name,
                '--no-lockscreen', '--width', '1000', '--height', '720'],
                env=kde_env, stdout=log, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 20
            while not (runtime / socket_name).exists():
                if kwin.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError('Owned nested KWin did not create its Wayland socket')
                time.sleep(.1)
            env.update({key: kde_env[key] for key in ['XDG_DATA_DIRS', 'XDG_CONFIG_DIRS', 'QT_PLUGIN_PATH', 'QML2_IMPORT_PATH']})
            env.update(WAYLAND_DISPLAY=socket_name, QT_QPA_PLATFORM='wayland')
            result['desktop'] = 'owned nested KWin Wayland compositor; no Plasma task-model/icon-pixel qualification'
            deadline = time.monotonic() + 20
            while True:
                check = subprocess.run([str(args.sysroot / 'usr/bin/qdbus6'), 'org.freedesktop.DBus', '/org/freedesktop/DBus', 'org.freedesktop.DBus.NameHasOwner', 'org.kde.KWin'], env=env, text=True, capture_output=True, timeout=5)
                if check.returncode == 0 and check.stdout.strip() == 'true': break
                if kwin.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError('Owned KWin socket exists but session-bus service did not register')
                time.sleep(.2)
        def launch(command, label):
            log = (args.out / (label + '.log')).open('wb'); logs.append(log)
            process = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT)
            owned.append(process)
            return process
        command = [str(binary), '--no-remote', '--profile', str(profile), '--marionette', 'about:blank']
        browser = launch(command, 'ui-browser')
        client = wait_client(args.port, profile, browser); release(client); client = None
        ui = subprocess.run([sys.executable, str(QA_ROOT / 'pwa_ui_probe.py'), '--port', str(args.port),
            '--fixture-port', str(args.fixture_port), '--out', str(args.out / 'ui-create')],
            env=env, capture_output=True, text=True, timeout=180)
        (args.out / 'ui-probe.log').write_text(ui.stdout + ui.stderr)
        result['ui'] = json.loads((args.out / 'ui-create/result.json').read_text())
        if ui.returncode or not result['ui']['completed']:
            raise RuntimeError('Actual full-package UI creation did not complete')
        client = wait_client(args.port, profile, browser)
        result['uiProduct'] = client.script(PRODUCT_SNAPSHOT)
        assert result['uiProduct']['sandbox'] == 6, 'Unexpected content sandbox level'
        assert result['uiProduct']['experiment'] is False, 'Rollout condition changed; do not force assignments'
        result['uiQuit'] = quit_owned(client, browser); client = browser = None
        entries = []
        apps = result['ui']['storeAfter']['apps']
        for app in sorted(apps, key=lambda item: item['name']):
            filename = 'floorp-' + app['id'].strip('{}').lower() + '.desktop'
            path = home / '.local/share/applications' / filename
            config = configparser.ConfigParser(interpolation=None); config.read(path)
            entry = config['Desktop Entry']; command = shlex.split(entry['Exec'])
            assert Path(command[0]).resolve() == binary.resolve(), 'Launcher belongs to another product'
            assert command[command.index('--profile') + 1] == str(profile), 'Launcher belongs to another profile'
            assert command[command.index('--start-ssb') + 1] == app['id']
            assert command[command.index('--name') + 1] == entry['StartupWMClass']
            entries.append({'app': app, 'exec': command, 'startupWMClass': entry['StartupWMClass'],
                'desktopSha256': hashlib.sha256(path.read_bytes()).hexdigest()})
        assert len(entries) == 2, 'Expected exactly two fresh UI-installed fixture apps'
        result['desktopEntries'] = entries
        server = http.server.ThreadingHTTPServer(('127.0.0.1', args.fixture_port), Fixture)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        result['coldCommands'] = []
        for index, entry in enumerate(entries):
            command = entry['exec'].copy()
            if index == 0:
                command.append('--marionette')
            result['coldCommands'].append(command)
            process = launch(command, 'cold-' + str(index))
            if index == 0:
                browser = process; client = wait_client(args.port, profile, browser)
            time.sleep(5)
        command = [str(binary), '--profile', str(profile), 'about:blank']
        result['coldCommands'].append(command); launch(command, 'cold-browser'); time.sleep(3)
        result['coldProduct'] = client.script(PRODUCT_SNAPSHOT)
        assert result['coldProduct']['sandbox'] == 6 and result['coldProduct']['experiment'] is False, 'Protection/rollout condition changed'
        pid = result['coldProduct']['pid']
        if args.mode == 'x11':
            tree = subprocess.run([str(args.sysroot / 'usr/bin/xwininfo'), '-root', '-tree'],
                env=env, capture_output=True, text=True, check=True, timeout=15)
            (args.out / 'x11-tree.txt').write_text(tree.stdout)
            result['x11Windows'] = []
            for xid, title in re.findall(r'^\s+(0x[0-9a-f]+) "([^"]*)"', tree.stdout, re.M):
                props = subprocess.run([str(args.sysroot / 'usr/bin/xprop'), '-id', xid,
                    'WM_CLASS', '_NET_WM_PID', '_NET_WM_NAME'], env=env, text=True, capture_output=True, timeout=15)
                if f'_NET_WM_PID(CARDINAL) = {pid}\n' not in props.stdout:
                    continue
                (args.out / (xid + '-xprop.txt')).write_text(props.stdout)
                match = re.search(r'WM_CLASS\(STRING\) = (.*)', props.stdout)
                result['x11Windows'].append({'xid': xid, 'title': title, 'wmClass': match[1] if match else None})
        else:
            marker = 'QA_FULLPACKAGE_PWA_' + str(pid)
            script = args.out / 'kwin-snapshot.js'
            script.write_text('print(' + json.dumps(marker + '=') +
                '+JSON.stringify(workspace.windowList().map(w=>({caption:w.caption,resourceClass:w.resourceClass,'
                'desktopFileName:w.desktopFileName,resourceName:w.resourceName,pid:w.pid}))));\n')
            name = 'qa-fullpackage-pwa-' + str(pid)
            qdbus = str(args.sysroot / 'usr/bin/qdbus6')
            script_id = subprocess.check_output([qdbus, 'org.kde.KWin', '/Scripting',
                'org.kde.kwin.Scripting.loadScript', str(script), name], env=env, text=True, timeout=15).strip()
            try:
                subprocess.run([qdbus, 'org.kde.KWin', '/Scripting/Script' + script_id,
                    'org.kde.kwin.Script.run'], env=env, check=True, capture_output=True, timeout=15)
            finally:
                subprocess.run([qdbus, 'org.kde.KWin', '/Scripting',
                    'org.kde.kwin.Scripting.unloadScript', name], env=env, check=True, capture_output=True, timeout=15)
            deadline = time.monotonic() + 5
            lines = []
            while not lines and time.monotonic() < deadline:
                lines = [line for line in args.kwin_log.read_text().splitlines() if marker + '=' in line]
                if not lines: time.sleep(.1)
            line = lines[-1]
            result['kwinWindows'] = [w for w in json.loads(line.split(marker + '=', 1)[1]) if w['pid'] == pid]
        native_windows = result.get('x11Windows', result.get('kwinWindows', []))
        result['pwaIdentityChecks'] = []
        for entry in entries:
            matches = [win for win in native_windows if entry['app']['name'] in win.get('title', win.get('caption', ''))]
            observed = []
            for win in matches:
                if args.mode == 'wayland':
                    observed.append(win['resourceClass'])
                else:
                    wm = re.fullmatch(r'"[^"]*", "([^"]*)"', win.get('wmClass') or '')
                    observed.append(wm[1] if wm else win.get('wmClass'))
            result['pwaIdentityChecks'].append({'name': entry['app']['name'],
                'expectedClass': entry['startupWMClass'], 'observedClasses': observed,
                'matches': len(observed) == 1 and observed[0] == entry['startupWMClass']})
        result['allPwaIdentitiesMatch'] = all(check['matches'] for check in result['pwaIdentityChecks'])
        result['coldQuit'] = quit_owned(client, browser); client = browser = None
        server.shutdown(); server.server_close(); server = None
        # Actual malformed APIs run in the same newly created dedicated profile,
        # against this full package. The probe itself performs normal in-app Quit.
        browser = launch([str(binary), '--no-remote', '--profile', str(profile), '--marionette', 'about:blank'], 'legacy-browser')
        client = wait_client(args.port, profile, browser); release(client); client = None
        legacy = subprocess.run([sys.executable, str(QA_ROOT / 'pwa_legacy_probe.py'), '--port', str(args.port),
            '--profile', str(profile), '--out', str(args.out / 'legacy-api.json')], env=env,
            text=True, capture_output=True, timeout=180)
        (args.out / 'legacy-api.log').write_text(legacy.stdout + legacy.stderr)
        result['legacyApi'] = json.loads((args.out / 'legacy-api.json').read_text())
        if legacy.returncode or 'calls' not in result['legacyApi']:
            raise RuntimeError('Full-package malformed API probe did not complete')
        browser.wait(timeout=20); browser = None
        result['completed'] = True
    except Exception as error:
        result['error'] = str(error)
    finally:
        if client:
            try:
                if browser: result['cleanupQuit'] = quit_owned(client, browser)
                else: release(client)
            except Exception as error:
                result['cleanupError'] = str(error)
        if server:
            server.shutdown(); server.server_close()
        for process in owned:
            if process.poll() is None:
                try:
                    cleanup_client = wait_client(args.port, profile, process)
                    result['cleanupQuit'] = quit_owned(cleanup_client, process)
                except Exception:
                    result.setdefault('ownedTerminationFallback', []).append(process.pid)
                    process.terminate(); process.wait(timeout=15)
        if kwin:
            kwin.terminate(); kwin.wait(timeout=15)
        if xvfb:
            xvfb.terminate(); xvfb.wait(timeout=15)
        result['ownedProcessExitCodes'] = [{'pid': process.pid, 'returncode': process.poll()} for process in owned]
        result['remainingOwnedProcesses'] = [process.pid for process in owned if process.poll() is None]
        if xvfb: result['ownedXvfbExitCode'] = xvfb.returncode
        if kwin: result['ownedKwinExitCode'] = kwin.returncode
        for log in logs: log.close()
        if args.mode == 'wayland':
            protocol = []
            for label in ['ui-browser.log', 'cold-0.log', 'cold-1.log']:
                path = args.out / label
                if path.exists():
                    protocol.extend(line for line in path.read_text(errors='replace').splitlines() if 'xdg_toplevel' in line and ('set_app_id' in line or 'set_title' in line))
            (args.out / 'wayland-app-id-protocol.txt').write_text('\n'.join(protocol) + '\n')
            result['nativeWaylandProtocolObserved'] = any('set_app_id' in line for line in protocol)
            if not result['nativeWaylandProtocolObserved']:
                result['completed'] = False; result['error'] = 'Native Wayland app_id protocol was not observed'
        after = inventory(product)
        result['productUnchanged'] = after == before
        if not result['productUnchanged']:
            result['completed'] = False; result['error'] = 'Full product files changed during QA'
        (args.out / 'product-inventory-after.json').write_text(json.dumps(after, indent=2))
        (args.out / 'result.json').write_text(json.dumps(result, indent=2))
    print(json.dumps({key: result.get(key) for key in ['completed', 'error', 'sourceSha', 'runId', 'productUnchanged']}))
    return 0 if result['completed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
