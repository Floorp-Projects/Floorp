#!/usr/bin/env python3
from pathlib import Path
import sys,json,hashlib,os
b=Path(__file__).resolve().parent;sys.path.insert(0,str(b.parent));from pwa_ui_probe import Marionette
m=Marionette('127.0.0.1',int(os.environ.get('QA_BRANDING_PORT','2829')));p=m.script("return Services.dirsvc.get('ProfD',Ci.nsIFile).path;");assert p==os.environ.get('QA_BRANDING_PROFILE',str(b/'profile')),p
bundle=(b/'colocated-bundle.js').read_text()
result=m.async_script('const done=arguments[arguments.length-1];'+bundle+'\nBrandingRegression.runAllTests().then(()=>done({pass:true,progress:globalThis.__NORA_TEST_PROGRESS__})).catch(e=>done({pass:false,error:String(e),progress:globalThis.__NORA_TEST_PROGRESS__}));')
result.update({'profile':p,'locale':m.script('return Services.locale.appLocaleAsBCP47;'),'bundleSha256':hashlib.sha256(bundle.encode()).hexdigest(),'execution':'complete colocated test module with repository test_harness runTests/assertEquals bundled by esbuild and executed in actual product chrome; top-level colocated orchestration runner not used'})
(b/(sys.argv[1]+'-regression.json')).write_text(json.dumps(result,indent=2));print(json.dumps(result));m.call('WebDriver:DeleteSession')
