#!/usr/bin/env python3
"""Call the product's own emitted settings module; never inject compiled source.

These are shipped-asset API cases, not a reproduction of two visible windows'
pending autosave timing. A real external pref writer represents the other
window, while the stale form object is obtained from the actual exported getter.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import zipfile

from full_ui import Runner


def discover(product):
    """Conservative emitted-function matching; fail instead of guessing exports."""
    found = []
    with zipfile.ZipFile(product / 'omni.ja') as archive:
        for name in archive.namelist():
            if not name.startswith('chrome/noraneko/nora-content-settings/assets/dataManager-') or not name.endswith('.js'):
                continue
            source = archive.read(name).decode()
            if 'floorp.design.configs' not in source or 'faviconColor' not in source:
                continue
            exported = re.search(r'export\{([^}]+)\};?\s*$', source)
            if not exported:
                continue
            bindings = dict(re.findall(r'([\w$]+) as ([\w$]+)', exported.group(1)))
            functions = list(re.finditer(r'(?:async )?function ([\w$]+)\(', source))
            bodies = {match.group(1): source[match.start():functions[index + 1].start() if index + 1 < len(functions) else len(source)]
                      for index, match in enumerate(functions)}
            getters = []
            savers = []
            for index, match in enumerate(functions):
                body = source[match.start():functions[index + 1].start() if index + 1 < len(functions) else len(source)]
                local = match.group(1)
                if local not in bindings:
                    continue
                if 'return{design:' in body and 'globalConfigs.userInterface' in body and 'tabOpenPosition:' in body:
                    getters.append((local, bindings[local]))
                wrapper = re.search(r'function [\w$]+\(([\w$]+)(?:,([\w$]+)=\{\})?\)\{return [\w$]+\(\(\)=>([\w$]+)\(([\w$,]+)\)\)\}', body)
                if wrapper:
                    target = bodies.get(wrapper.group(3), '')
                    # Verify that the exported queued function reaches the actual
                    # design save body, rather than assuming every queued export is it.
                    semantic_save = all(token in target for token in ['tabbarStyle:', 'faviconColor:', '.setStringPref('])
                    if semantic_save:
                        savers.append((local, bindings[local], wrapper.group(3), bool(wrapper.group(2))))
            if len(getters) == 1 and len(savers) == 1:
                found.append({'archive': 'omni.ja', 'entry': name,
                              'sha256': hashlib.sha256(source.encode()).hexdigest(),
                              'uri': 'chrome://noraneko-settings/content/assets/' + Path(name).name,
                              'getLocal': getters[0][0], 'getExport': getters[0][1],
                              'saveLocal': savers[0][0], 'saveExport': savers[0][1],
                              'saveInternalLocal': savers[0][2], 'saveAcceptsOptions': savers[0][3],
                              'exportDeclaration': exported.group(0)})
    if len(found) != 1:
        raise RuntimeError('Cannot uniquely prove shipped get/save exports: ' + repr(found))
    return found[0]


class ShippedRunner(Runner):
    def invoke(self, export, settings=None, options=None):
        self.client.call('Marionette:SetContext', {'value': 'content'})
        return self.client.async_script(r"""
const done=arguments[arguments.length-1];
const [uri,key,settings,options]=arguments;
import(uri).then(module=>{
 if(typeof module[key]!=="function")throw new Error("Missing shipped export "+key);
 return settings===null?module[key]():module[key](settings,options);
}).then(value=>done({ok:true,value:value??null}),error=>done({ok:false,error:String(error),stack:error.stack}));
""", [self.module['uri'], export, settings, options])

    def checked_invoke(self, export, settings=None, options=None):
        value = self.invoke(export, settings, options)
        assert value['ok'], value
        return value['value']

    def shipped_api(self):
        self.module = discover(self.product)
        self.save('shipped-module-discovery', self.module)
        self.start_browser('01-shipped-api')
        self.hub('02-actual-hub-origin')
        self.cases = []
        original = self.script("return {design:Services.prefs.getStringPref('floorp.design.configs'),vertical:Services.prefs.getBoolPref('sidebar.verticalTabs')};", 'chrome')
        try:
            for initial, latest in [('horizontal', 'multirow'), ('multirow', 'horizontal')]:
                current = self.checked_invoke(self.module['getExport'])
                assert current is not None
                self.checked_invoke(self.module['saveExport'], {**current, 'style': initial}, {'hasTabStyleChanged': True})
                stale = self.checked_invoke(self.module['getExport'])
                assert stale['style'] == initial, stale
                # Actual external Services.prefs write, not a mock; only local dedicated profile.
                self.client.call('Marionette:SetContext', {'value': 'chrome'})
                external = self.client.script(r"""
const next=arguments[0],config=JSON.parse(Services.prefs.getStringPref('floorp.design.configs'));
config.tabbar.tabbarStyle=next;
Services.prefs.setStringPref('floorp.design.configs',JSON.stringify(config));
return {style:JSON.parse(Services.prefs.getStringPref('floorp.design.configs')).tabbar.tabbarStyle,native:Services.prefs.getBoolPref('sidebar.verticalTabs')};
""", [latest])
                assert external['style'] == latest and external['native'] is False, external
                favicon = not stale['faviconColor']
                self.checked_invoke(self.module['saveExport'], {**stale, 'faviconColor': favicon}, {'hasTabStyleChanged': False})
                observed = self.script("const c=JSON.parse(Services.prefs.getStringPref('floorp.design.configs'));return {style:c.tabbar.tabbarStyle,faviconColor:c.globalConfigs.faviconColor,native:Services.prefs.getBoolPref('sidebar.verticalTabs')};", 'chrome')
                self.cases.append({'id': 'stale-' + initial + '-latest-' + latest,
                                   'staleFromActualShippedGetter': stale, 'externalActualPrefWriter': external,
                                   'unrelatedFaviconChange': favicon, 'observed': observed,
                                   'pass': observed['style'] == latest and observed['faviconColor'] == favicon and observed['native'] is False})
                self.save('shipped-api-cases', self.cases)
        finally:
            self.client.call('Marionette:SetContext', {'value': 'chrome'})
            self.client.script("Services.prefs.setBoolPref('sidebar.verticalTabs',arguments[0].vertical);Services.prefs.setStringPref('floorp.design.configs',arguments[0].design);return true;", [original])
        self.navigate('about:blank')
        self.quit('03-normal-quit')

    def evaluate(self):
        return {case['id']: case['pass'] for case in self.cases}


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--product', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    args.mode = 'shipped_api'
    args.migration_from = None
    sys.exit(ShippedRunner(args).run())
