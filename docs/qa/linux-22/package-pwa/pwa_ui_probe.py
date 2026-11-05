#!/usr/bin/env python3
"""Actual bundled Floorp PWA UI probe; synthetic Marionette input, not physical input.

Launch a *fresh dedicated product profile* beforehand with --marionette
--remote-allow-system-access. Do not lower sandbox levels or force experiments.
Run this within the same network namespace as the browser.
This only creates the two local fixture apps in that dedicated profile.
It does not alter preferences, install extensions, seed the SSB store or replace UI.
"""
import argparse
import base64
import http.server
import json
from pathlib import Path
import socket
import struct
import threading
import time
import traceback
import zlib


class Marionette:
    def __init__(self, host, port):
        self.socket = socket.create_connection((host, port), timeout=30)
        self.socket.settimeout(60)
        self.sequence = 0
        self.hello = self.receive()
        self.call('WebDriver:NewSession', {'capabilities': {}})
        self.call('Marionette:SetContext', {'value': 'chrome'})

    def receive(self):
        size = b''
        while True:
            char = self.socket.recv(1)
            if not char:
                raise EOFError('Marionette closed the connection')
            if char == b':':
                break
            size += char
        remaining = int(size)
        data = b''
        while remaining:
            part = self.socket.recv(remaining)
            if not part:
                raise EOFError('Incomplete Marionette packet')
            data += part
            remaining -= len(part)
        return json.loads(data)

    def call(self, name, args=None):
        self.sequence += 1
        payload = json.dumps([0, self.sequence, name, args or {}]).encode()
        self.socket.sendall(str(len(payload)).encode() + b':' + payload)
        result = self.receive()
        if result[2]:
            raise RuntimeError(f'{name}: {result[2]}')
        return result[3]

    def script(self, script, args=None):
        result = self.call('WebDriver:ExecuteScript', {
            'script': script, 'args': args or [], 'newSandbox': True,
            'sandbox': 'default', 'line': 1, 'filename': 'pwa_ui_probe.py',
        })
        return result.get('value', result)

    def async_script(self, script, args=None):
        result = self.call('WebDriver:ExecuteAsyncScript', {
            'script': script, 'args': args or [], 'newSandbox': True,
            'sandbox': 'default', 'line': 1, 'filename': 'pwa_ui_probe.py',
        })
        return result.get('value', result)

    def click(self, selector):
        element = self.call('WebDriver:FindElement', {'using': 'css selector', 'value': selector})
        element = element.get('value', element)
        element_id = element.get('element-6066-11e4-a52e-4f735466cecf', element.get('ELEMENT'))
        if not element_id:
            raise RuntimeError(f'No element reference for {selector}: {element}')
        self.call('WebDriver:ElementClick', {'id': element_id})


def png(red, green, blue):
    def chunk(kind, data):
        return struct.pack('!I', len(data)) + kind + data + struct.pack('!I', zlib.crc32(kind + data))
    raw = b''.join(b'\x00' + bytes((red, green, blue, 255)) * 192 for _ in range(192))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('!2I5B', 192, 192, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')


class Fixture(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        name = 'Alpha' if self.path.startswith('/alpha/') else 'Beta'
        prefix = '/' + name.lower() + '/'
        if self.path.endswith('.webmanifest'):
            data = json.dumps({'name': 'QA PWA ' + name, 'short_name': name,
                'start_url': prefix, 'scope': prefix, 'display': 'standalone',
                'icons': [{'src': prefix + 'icon.png', 'sizes': '192x192', 'type': 'image/png'}]}).encode()
            content_type = 'application/manifest+json'
        elif self.path.endswith('.png'):
            data = png(220, 40, 50) if name == 'Alpha' else png(30, 90, 220)
            content_type = 'image/png'
        else:
            data = (f'<!doctype html><meta charset="utf-8"><title>QA PWA {name}</title>'
                f'<link rel="manifest" href="{prefix}app.webmanifest">'
                f'<link rel="icon" href="{prefix}icon.png">'
                f'<h1>QA PWA {name}</h1><p>Local isolated fixture 日本語の文字 漢字 かな カナ</p>').encode()
            content_type = 'text/html; charset=utf-8'
        self.send_response(200)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)


SNAPSHOT = r"""
const w = window;
const b = w.gBrowser.selectedBrowser;
const ids = ['ssbPageAction', 'ssb-panel', 'ssb-app-install-button',
 'appMenu-ssb-button', 'appMenu-install-or-open-ssb-current-page-button'];
const elements = Object.fromEntries(ids.map(id => {
 const e=w.document.getElementById(id); const r=e?.getBoundingClientRect();
 return [id,e ? {hidden:e.hidden,disabled:e.disabled,state:e.state,
  label:e.getAttribute('label'),rect:r?{x:r.x,y:r.y,width:r.width,height:r.height}:null}:null];
}));
let actor;
try { actor={available:!!b.browsingContext.currentWindowGlobal.getActor('NRProgressiveWebApp')}; }
catch(e) { actor={available:false,error:String(e)}; }
let experiment;
try { experiment=ChromeUtils.importESModule('resource://noraneko/modules/pwa/TaskbarExperiment.sys.mjs').TaskbarExperiment.isEnabledForCurrentPlatform(); }
catch(e) { experiment={error:String(e)}; }
return {product:{name:Services.appinfo.name,version:Services.appinfo.version,
 platformVersion:Services.appinfo.platformVersion,buildID:Services.appinfo.appBuildID},
 profile:Services.dirsvc.get('ProfD',Ci.nsIFile).path,
 remoteType:b.remoteType,url:b.currentURI.spec,actor,elements,experiment,
 sandbox:Services.prefs.getIntPref('security.sandbox.content.level',-1),
 windows:Array.from(Services.wm.getEnumerator(null),win=>({title:win.document.title,
 type:win.document.documentElement.getAttribute('windowtype'),
 ssbid:win.document.documentElement.getAttribute('ssbid'),
 url:win.gBrowser?.selectedBrowser?.currentURI?.spec}))};
"""

STORE = r"""
const done=arguments[arguments.length-1];
const path=PathUtils.join(PathUtils.profileDir,'ssb','ssb.json');
IOUtils.exists(path).then(exists=>exists?IOUtils.readJSON(path):{}).then(data=>
done({path,apps:Object.values(data).map(({id,name,start_url,userContextId})=>({id,name,start_url,userContextId}))}),
error=>done({path,error:String(error)}));
"""


def wait(predicate, seconds=30):
    deadline = time.monotonic() + seconds
    last = None
    while time.monotonic() < deadline:
        try:
            last = predicate()
            if last:
                return last
        except Exception as error:
            last = str(error)
        time.sleep(.3)
    raise TimeoutError(f'Condition did not become true: {last}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', default='127.0.0.1')
    parser.add_argument('--port', type=int, default=2828)
    parser.add_argument('--fixture-port', type=int, default=8765)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--inspect-only', action='store_true')
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    report = {'input': 'synthetic Marionette element clicks; no physical-input claim',
              'scope': 'actual unmodified packaged product UI, local fixture', 'steps': []}
    browser = None
    server = None
    try:
        if not args.inspect_only:
            server = http.server.ThreadingHTTPServer(('127.0.0.1', args.fixture_port), Fixture)
            threading.Thread(target=server.serve_forever, daemon=True).start()
        browser = Marionette(args.host, args.port)
        report['hello'] = browser.hello
        report['before'] = browser.script(SNAPSHOT)
        report['storeBefore'] = browser.async_script(STORE)
        if not args.inspect_only:
            for name, route in [('alpha','urlbar'),('beta','menu')]:
                step = {'fixture': name, 'route': route}
                report['steps'].append(step)
                url = f'http://127.0.0.1:{args.fixture_port}/{name}/'
                # Browser navigation is controlled through normal browser APIs;
                # install itself always uses the visible packaged UI.
                browser.script("const w=window; w.gBrowser.selectedTab=w.gBrowser.addTrustedTab(arguments[0]); return true;", [url])
                wait(lambda: browser.script("return window.gBrowser.selectedBrowser.currentURI.spec;") == url)
                time.sleep(3)
                step['preInstall'] = browser.script(SNAPSHOT)
                if route == 'urlbar':
                    wait(lambda: browser.script("return !!document.getElementById('ssbPageAction')?.getBoundingClientRect().width;"))
                    browser.click('#ssbPageAction')
                    wait(lambda: browser.script("return document.getElementById('ssb-panel')?.state === 'open';"))
                    browser.click('#ssb-app-install-button')
                else:
                    browser.click('#PanelUI-menu-button')
                    wait(lambda: browser.script("return !!document.getElementById('appMenu-ssb-button')?.getBoundingClientRect().width;"))
                    browser.click('#appMenu-ssb-button')
                    wait(lambda: browser.script("const e=document.getElementById('appMenu-install-or-open-ssb-current-page-button'); return !!e?.getBoundingClientRect().width && !e.disabled;"))
                    browser.click('#appMenu-install-or-open-ssb-current-page-button')
                step['store'] = wait(lambda: (lambda data: data if any(app['name'] == 'QA PWA ' + name.title() for app in data.get('apps',[])) else None)(browser.async_script(STORE)))
                time.sleep(5)
                step['afterInstall'] = browser.script(SNAPSHOT)
                try:
                    shot = browser.call('WebDriver:TakeScreenshot', {'full': False})
                    image_data = shot.get('value', shot)
                    (args.out / f'{name}-after.png').write_bytes(base64.b64decode(image_data))
                except Exception as error:
                    step['screenshotError'] = str(error)
        report['after'] = browser.script(SNAPSHOT)
        report['storeAfter'] = browser.async_script(STORE)
        report['console'] = browser.script("return Services.console.getMessageArray().map(m=>m.message).filter(x=>/PWA|ssb|actor|Taskbar|LinuxSupport/i.test(x)).slice(-100);")
        report['completed'] = True
    except Exception as error:
        report['completed'] = False
        report['error'] = str(error)
        report['traceback'] = traceback.format_exc()
        if browser:
            try:
                report['failureSnapshot'] = browser.script(SNAPSHOT)
                report['failureStore'] = browser.async_script(STORE)
                report['console'] = browser.script("return Services.console.getMessageArray().map(m=>m.message).slice(-100);")
            except Exception as snapshot_error:
                report['snapshotError'] = str(snapshot_error)
    finally:
        if server:
            server.shutdown()
        if browser:
            try:
                browser.call('WebDriver:DeleteSession')
            except Exception:
                pass
            browser.socket.close()
        (args.out / 'result.json').write_text(json.dumps(report, indent=2, ensure_ascii=False))
    print(json.dumps({'completed':report.get('completed'),'error':report.get('error'),'out':str(args.out)}))
    return 0 if report.get('completed') else 1


if __name__ == '__main__':
    raise SystemExit(main())
