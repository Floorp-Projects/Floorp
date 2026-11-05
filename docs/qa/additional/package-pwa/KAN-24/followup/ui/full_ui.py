#!/usr/bin/env python3
"""Exercise immutable complete Floorp products with genuine synthetic UI input.

Run within dbus-run-session. Xvfb102/Marionette2914 are owned by this runner.
Baseline mode creates a naturally mismatched profile using visible controls and
normal quit. Candidate mode accepts a copy of that profile, checks startup on
about:blank before ever navigating to Hub, then exercises live roundtrips,
new windows, Workspace switches, multirow and a normal restart. It does not
seed any product configuration preference, replace resources, disable sandbox,
claim physical input or qualify macOS/KDE/Wayland/GPU behavior.
"""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import traceback

BASE = Path('/workspace/qa-linux-22/package-pwa')
SYSROOT = BASE / 'sysroot'
sys.path.insert(0, str(BASE))
from pwa_ui_probe import Marionette

SNAPSHOT = r"""
const windows=Array.from(Services.wm.getEnumerator('navigator:browser'));
const rect=e=>{const r=e.getBoundingClientRect();return {x:r.x,y:r.y,width:r.width,height:r.height}};
return {version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,
 executable:Services.dirsvc.get('XREExeF',Ci.nsIFile).path,
 profile:Services.dirsvc.get('ProfD',Ci.nsIFile).path,
 sandbox:Services.prefs.getIntPref('security.sandbox.content.level'),
 sandboxDisabled:Services.env.exists('MOZ_DISABLE_CONTENT_SANDBOX'),
 vertical:Services.prefs.getBoolPref('sidebar.verticalTabs'),
 config:JSON.parse(Services.prefs.getStringPref('floorp.design.configs','{}')),
 windows:windows.map(w=>({ready:w.gBrowserInit.delayedStartupFinished,
  url:w.gBrowser.selectedBrowser.currentURI.spec,vertical:w.gBrowser.tabContainer.verticalMode,
  tabURLs:Array.from(w.gBrowser.tabs,t=>t.linkedBrowser.currentURI.spec),
  orient:w.gBrowser.tabContainer.getAttribute('orient'),rect:rect(w.gBrowser.tabContainer)}))};
"""
HUB_STATE = "return {style:document.querySelector('input[name=style]:checked')?.value,design:document.querySelector('input[name=design]:checked')?.value,url:location.href};"


def inventory(product):
    return {str(f.relative_to(product)): hashlib.sha256(f.read_bytes()).hexdigest()
            for f in sorted(product.rglob('*')) if f.is_file()}


class Runner:
    def __init__(self, args):
        self.args = args
        self.out = args.out.resolve()
        self.out.mkdir(parents=True, exist_ok=False)
        self.product = args.product.resolve(strict=True)
        self.profile = self.out / 'profile'
        if args.migration_from:
            shutil.copytree(args.migration_from.resolve(strict=True), self.profile,
                            ignore=shutil.ignore_patterns('.parentlock', 'parent.lock', 'lock'))
        else:
            self.profile.mkdir()
        # The sole launch override selects the automation endpoint; no design pref is seeded.
        (self.profile / 'user.js').write_text('user_pref("marionette.port",2914);\n')
        self.home = self.out / 'home'
        self.home.mkdir()
        self.runtime = self.out / 'runtime'
        self.runtime.mkdir(mode=0o700)
        self.env = dict(os.environ, HOME=str(self.home), XDG_CONFIG_HOME=str(self.home / '.config'),
                        XDG_DATA_HOME=str(self.home / '.local/share'), XDG_CACHE_HOME=str(self.home / '.cache'),
                        XDG_RUNTIME_DIR=str(self.runtime), DISPLAY=':102', GDK_BACKEND='x11',
                        MOZ_ENABLE_WAYLAND='0', MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',
                        LD_LIBRARY_PATH=str(SYSROOT / 'usr/lib/x86_64-linux-gnu'))
        for name in list(self.env):
            if name.startswith('MOZ_DISABLE_') or name == 'WAYLAND_DISPLAY':
                self.env.pop(name)
        self.client = None
        self.browser = None
        self.display = None
        self.logs = []
        self.records = []
        self.quits = []
        self.before = inventory(self.product)
        self.save('product-hashes-before', self.before)
        self.save('product-ini-metadata', {name: (self.product / name).read_text()
                                         for name in ['application.ini', 'platform.ini']})

    def save(self, name, value):
        (self.out / (name + '.json')).write_text(json.dumps(value, indent=2))
        return value

    def log_process(self, name, command):
        handle = (self.out / (name + '.log')).open('w')
        self.logs.append(handle)
        return subprocess.Popen(command, env=self.env, stdout=handle, stderr=subprocess.STDOUT)

    def start_display(self):
        assert not Path('/tmp/.X102-lock').exists() and not Path('/tmp/.X11-unix/X102').exists(), 'display102 already occupied'
        self.display = self.log_process('xvfb', [str(SYSROOT / 'usr/bin/Xvfb'), ':102', '-screen', '0', '1280x900x24', '-nolisten', 'tcp'])
        for _ in range(60):
            assert self.display.poll() is None, 'owned Xvfb failed'
            if Path('/tmp/.X11-unix/X102').exists():
                return
            time.sleep(.2)
        raise RuntimeError('Xvfb102 socket unavailable')

    def start_browser(self, label):
        self.browser = self.log_process(label, [str(self.product / 'floorp'), '--no-remote', '--profile', str(self.profile), '--marionette', 'about:blank'])
        for _ in range(180):
            assert self.browser.poll() is None, 'browser exited before Marionette'
            try:
                self.client = Marionette('127.0.0.1', 2914)
                break
            except OSError:
                time.sleep(.25)
        else:
            raise RuntimeError('Marionette unavailable')
        self.wait('chrome', "return !!window.gBrowserInit?.delayedStartupFinished && Services.prefs.prefHasUserValue('floorp.design.configs');")
        # about:blank argument and selected URLs recorded, never assume absence of restored Hub.
        snapshot = self.record(label + '-startup-before-hub', SNAPSHOT, 'chrome', screenshot=True)
        assert snapshot['sandbox'] == 6 and not snapshot['sandboxDisabled']
        assert snapshot['version'] == '159.0.1' and snapshot['buildID'] == '20261103153232', 'unexpected live locked Runtime identity'
        assert Path(snapshot['executable']).resolve() == (self.product / 'floorp').resolve()
        assert snapshot['profile'] == str(self.profile)
        if label in ['08-baseline-restart', '01-candidate']:
            assert all(not url.startswith('about:hub') for w in snapshot['windows'] for url in w['tabURLs']), 'Hub restored before intended cold-start observation'
        return snapshot

    def script(self, script, context='content'):
        self.client.call('Marionette:SetContext', {'value': context})
        return self.client.script(script)

    def wait(self, context, script, timeout=35):
        deadline = time.monotonic() + timeout
        last = None
        while time.monotonic() < deadline:
            last = self.script(script, context)
            if last:
                return last
            time.sleep(.3)
        raise RuntimeError('UI readiness timeout: ' + str(last))

    def record(self, label, script=SNAPSHOT, context='chrome', screenshot=False):
        value = self.save(label, self.script(script, context))
        self.records.append({'name': label, 'file': label + '.json'})
        if screenshot:
            image = self.client.call('WebDriver:TakeScreenshot', {'full': False})['value']
            (self.out / (label + '.png')).write_bytes(base64.b64decode(image))
        print(label, json.dumps(value)[:260], flush=True)
        return value

    def navigate(self, url):
        self.client.call('Marionette:SetContext', {'value': 'content'})
        self.client.call('WebDriver:Navigate', {'url': url})
        time.sleep(1)

    def hub(self, label):
        self.navigate('about:hub#/features/design')
        self.wait('content', "return !!document.querySelector('input[name=style]:checked');")
        self.script("document.querySelector('input[name=style]:checked').scrollIntoView({block:'center'});return true;", 'content')
        return self.record(label, HUB_STATE, 'content', screenshot=True)

    def click(self, selector, context='content'):
        self.client.call('Marionette:SetContext', {'value': context})
        self.wait(context, 'return !!document.querySelector(' + json.dumps(selector) + ');')
        self.client.click(selector)
        time.sleep(1)

    def choose_style(self, style, label):
        self.click('input[name=style][value=' + style + ']')
        self.script("document.querySelector('input[name=style]:checked').scrollIntoView({block:'center'});return true;", 'content')
        self.record(label + '-hub', HUB_STATE, 'content', screenshot=True)
        return self.record(label + '-render')

    def native(self, vertical, label):
        self.navigate('about:preferences#tabsBrowsing')
        identity = 'browserLayoutVerticalTabs' if vertical else 'browserLayoutHorizontalTabs'
        expression = 'document.getElementById(' + json.dumps(identity) + ')?.shadowRoot?.querySelector("button")'
        self.wait('content', 'const b=' + expression + ';return !!b && b.getBoundingClientRect().width>0;')
        self.script('const b=' + expression + ';b.scrollIntoView({block:"center"});return true;')
        element = self.script('return ' + expression + ';')
        self.client.call('WebDriver:ElementClick', {'id': element['element-6066-11e4-a52e-4f735466cecf']})
        time.sleep(1)
        self.record(label + '-native-control', "return {horizontal:document.getElementById('browserLayoutHorizontalTabs').checked,vertical:document.getElementById('browserLayoutVerticalTabs').checked};", 'content', screenshot=True)
        return self.record(label + '-render')

    def quit(self, label):
        response = self.client.call('Marionette:Quit', {'flags': ['eAttemptQuit']})
        self.client.socket.close()
        self.client = None
        code = self.browser.wait(timeout=40)
        result = {'response': response, 'exitCode': code, 'forced': False}
        self.quits.append(self.save(label, result))
        assert code == 0

    def baseline(self):
        self.start_browser('01-initial')
        self.hub('02-initial-hub')
        self.click('label:has(input[name=design][value=photon])')
        reverse = self.args.migration_direction == 'vertical'
        self.choose_style('horizontal' if reverse else 'vertical', '03-hub-to-native')
        self.native(reverse, '04-native-on' if reverse else '04-native-off')
        self.hub('05-hub-after-on' if reverse else '05-hub-after-off')
        self.record('06-natural-mismatch')
        # Make the migration source cold-open a blank tab; it has never been pref seeded.
        self.navigate('about:blank')
        self.wait('chrome', "return gBrowser.selectedBrowser.currentURI.spec==='about:blank';")
        self.quit('07-normal-quit')
        shutil.copytree(self.profile, self.out / 'migration-seed', ignore=shutil.ignore_patterns('.parentlock', 'parent.lock', 'lock'))
        self.start_browser('08-baseline-restart')
        self.hub('09-baseline-restarted-hub')
        self.navigate('about:blank')
        self.quit('10-normal-quit')

    def candidate(self):
        self.start_browser('01-candidate')
        self.hub('02-migration-hub')
        self.click('label:has(input[name=design][value=photon])')
        self.choose_style('vertical', '03-hub-to-native')
        self.native(False, '04-native-off')
        self.hub('05-hub-after-off')
        self.native(True, '06-native-on')
        self.hub('07-hub-after-on')
        self.click('#PanelUI-menu-button', 'chrome')
        self.click('#appMenu-new-window-button2', 'chrome')
        self.wait('chrome', "const ws=Array.from(Services.wm.getEnumerator('navigator:browser'));return ws.length===2&&ws.every(w=>w.gBrowserInit.delayedStartupFinished&&w.gBrowser.selectedBrowser.currentURI.spec!=='about:blank');")
        self.record('08-new-window-ready', screenshot=True)
        # Keep the active WebDriver window; close only the other product window.
        self.script("for(const w of Services.wm.getEnumerator('navigator:browser'))if(w!==window)w.close();return true;", 'chrome')
        self.wait('chrome', "return Array.from(Services.wm.getEnumerator('navigator:browser')).length===1;")
        self.hub('09-original-window-hub')
        self.choose_style('multirow', '10-multirow')
        self.click('#workspaces-toolbar-button', 'chrome')
        self.click('#workspacesCreateNewWorkspaceButton', 'chrome')
        self.click('#workspaces-toolbar-button', 'chrome')
        self.click('.workspaceButton:first-child', 'chrome')
        self.record('11-workspace-first')
        self.click('#workspaces-toolbar-button', 'chrome')
        self.click('.workspaceButton:nth-child(2)', 'chrome')
        self.record('12-workspace-second')
        self.click('#workspaces-toolbar-button', 'chrome')
        self.click('.workspaceButton:first-child', 'chrome')
        self.record('13-workspace-return')
        self.script('gBrowser.selectedTab=gBrowser.tabs[0];return true;', 'chrome')
        self.hub('14-multirow-after-workspace')
        self.navigate('about:blank')
        self.quit('15-normal-quit')
        self.start_browser('16-normal-restart')
        self.hub('17-multirow-after-restart')
        self.navigate('about:blank')
        self.quit('18-final-normal-quit')

    def evaluate(self):
        def read(name):
            return json.loads((self.out / (name + '.json')).read_text())

        def stored(record):
            return record.get('config', {}).get('tabbar', {}).get('tabbarStyle')

        if self.args.mode == 'baseline':
            mismatch = read('06-natural-mismatch')
            restarted = read('08-baseline-restart-startup-before-hub')
            reverse = self.args.migration_direction == 'vertical'
            prior_style = 'horizontal' if reverse else 'vertical'
            return {
                'hubToNativeMatchesChosenInitialStyle': read('03-hub-to-native-render')['vertical'] is (not reverse),
                'naturalMismatchReproduced': mismatch['vertical'] is reverse and stored(mismatch) == prior_style and read('05-hub-after-on' if reverse else '05-hub-after-off')['style'] == prior_style,
                'actualTabstripMatchesNative': all(w['vertical'] is reverse for w in mismatch['windows']),
                'mismatchSurvivesNormalRestart': restarted['vertical'] is reverse and stored(restarted) == prior_style,
                'restartSelectedURLsAllBlank': all(w['url'] == 'about:blank' for w in restarted['windows']),
                'restartAllTabsNoHub': all(not url.startswith('about:hub') for w in restarted['windows'] for url in w['tabURLs']),
            }
        startup = read('01-candidate-startup-before-hub')
        expected_vertical = self.args.migration_direction == 'vertical'
        expected_style = self.args.migration_direction
        return {
            'naturalMigrationProfileProvided': self.args.migration_from is not None,
            'startupReconciledBeforeHubNavigation': startup['vertical'] is expected_vertical and stored(startup) == expected_style,
            'startupSelectedURLsAllBlank': all(w['url'] == 'about:blank' for w in startup['windows']),
            'startupAllTabsNoHub': all(not url.startswith('about:hub') for w in startup['windows'] for url in w['tabURLs']),
            'hubToNativeVertical': read('03-hub-to-native-render')['vertical'] is True,
            'nativeOffToHubHorizontal': read('04-native-off-render')['vertical'] is False and stored(read('04-native-off-render')) == 'horizontal' and read('05-hub-after-off')['style'] == 'horizontal',
            'nativeOnToHubVertical': read('06-native-on-render')['vertical'] is True and stored(read('06-native-on-render')) == 'vertical' and read('07-hub-after-on')['style'] == 'vertical',
            'settledNewWindowsVertical': len(read('08-new-window-ready')['windows']) == 2 and all(w['ready'] and w['vertical'] for w in read('08-new-window-ready')['windows']),
            'workspaceMultirowPreserved': all(read(name)['vertical'] is False and stored(read(name)) == 'multirow' for name in ['11-workspace-first', '12-workspace-second', '13-workspace-return']) and read('14-multirow-after-workspace')['style'] == 'multirow',
            'normalRestartMultirowPreserved': read('16-normal-restart-startup-before-hub')['vertical'] is False and stored(read('16-normal-restart-startup-before-hub')) == 'multirow' and read('17-multirow-after-restart')['style'] == 'multirow',
        }

    def run(self):
        result = {'mode': self.args.mode, 'migrationDirection': getattr(self.args, 'migration_direction', None), 'product': str(self.product), 'input': 'synthetic real WebDriver ElementClick, native shadow buttons', 'display': 'Xvfb102 X11', 'fullProductResourceMutation': False, 'originalMacOSQualified': False}
        try:
            self.start_display()
            getattr(self, self.args.mode)()
            result['completed'] = True
            result['checks'] = self.evaluate()
            result['allExpectedChecksPass'] = all(result['checks'].values())
        except Exception as error:
            result['completed'] = False
            result['error'] = str(error)
            result['traceback'] = traceback.format_exc()
        finally:
            if self.client:
                try:
                    self.quit('failure-normal-quit')
                except Exception as error:
                    result['failureQuitError'] = str(error)
            for proc in [self.browser, self.display]:
                if proc and proc.poll() is None:
                    proc.terminate()
                    try:
                        proc.wait(timeout=15)
                    except subprocess.TimeoutExpired:
                        proc.kill()
                        proc.wait()
            for handle in self.logs:
                handle.close()
            after = inventory(self.product)
            self.save('product-hashes-after', after)
            result['productUnchanged'] = self.before == after
            result['normalQuits'] = self.quits
            result['ownedProcessesRemaining'] = sum(proc is not None and proc.poll() is None for proc in [self.browser, self.display])
            result['records'] = self.records
            self.save('result', result)
            print(json.dumps(result), flush=True)
        return 0 if result['completed'] and result['productUnchanged'] else 1


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--product', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--mode', choices=['baseline', 'candidate'], required=True)
    parser.add_argument('--migration-from', type=Path)
    parser.add_argument('--migration-direction', choices=['horizontal', 'vertical'], default='horizontal',
                        help='Native target direction of the natural mismatch; horizontal means nativefalse/storedvertical.')
    arguments = parser.parse_args()
    sys.exit(Runner(arguments).run())
