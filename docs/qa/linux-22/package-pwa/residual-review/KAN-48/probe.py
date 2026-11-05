#!/usr/bin/env python3
"""Actual original-site reachability/PWA probe; no certificate/experiment bypass."""
import os,sys,json,time,subprocess,hashlib,base64,traceback,urllib.parse
from pathlib import Path
D=Path(__file__).resolve().parent;B=Path('/workspace/qa-linux-22/package-pwa');sys.path.insert(0,str(B));from pwa_ui_probe import Marionette,SNAPSHOT,STORE
P=Path('/workspace/qa-linux-additional/KAN-24/followup/products/baseline/floorp');s=json.loads((D/'session.json').read_text());e=dict(os.environ,**s['env']);e.update(MOZ_ENABLE_WAYLAND='1',GDK_BACKEND='wayland',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',WAYLAND_DEBUG='client')
for k in list(e):
 if k.startswith('MOZ_DISABLE_'):del e[k]
profile=D/'profile';profile.mkdir(exist_ok=False)
prefs=['user_pref("marionette.port",2926);']
proxy=urllib.parse.urlsplit(os.environ.get('HTTPS_PROXY',os.environ.get('HTTP_PROXY','')))
if proxy.hostname:
 prefs += ['user_pref("network.proxy.type",1);',f'user_pref("network.proxy.http",{json.dumps(proxy.hostname)});',f'user_pref("network.proxy.ssl",{json.dumps(proxy.hostname)});',f'user_pref("network.proxy.http_port",{proxy.port or 8080});',f'user_pref("network.proxy.ssl_port",{proxy.port or 8080});','user_pref("network.proxy.no_proxies_on","localhost, 127.0.0.1");']
(profile/'user.js').write_text('\n'.join(prefs)+'\n')
def inventory():return {str(p.relative_to(P)):hashlib.sha256(p.read_bytes()).hexdigest() for p in P.rglob('*') if p.is_file()}
before=inventory();(D/'product-hashes-before.json').write_text(json.dumps(before,indent=2));result={'sourceSha':'29a3eb23145da90706d8a94441373278521cc737','packageRun':42093687541,'artifact':13446089131,'input':'real product navigation, synthetic WebDriver; no physical-input claim','environment':'Debian13.6/native nestedKWin6.3.6/X106/normalDBus; no originalCachyOS qualification','sites':[],'proxy':'inherited session proxy selected through normal per-profile network prefs; no credentials printed','acceptInsecureCerts':False};m=None;proc=None
try:
 proc=subprocess.Popen([str(P/'floorp'),'--no-remote','--profile',str(profile),'--marionette','about:blank'],env=e,stdout=(D/'browser-wayland.log').open('w'),stderr=subprocess.STDOUT)
 for i in range(160):
  if proc.poll() is not None:raise RuntimeError('product exited early')
  try:m=Marionette('127.0.0.1',2926);break
  except OSError:time.sleep(.25)
 assert m,'no Marionette'
 m.call('WebDriver:SetTimeouts',{'pageLoad':20000,'script':30000})
 result['identity']=m.script('return {version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,profile:Services.dirsvc.get("ProfD",Ci.nsIFile).path,sandbox:Services.prefs.getIntPref("security.sandbox.content.level"),disable:Services.env.exists("MOZ_DISABLE_CONTENT_SANDBOX"),experiment:ChromeUtils.importESModule("resource://noraneko/modules/pwa/TaskbarExperiment.sys.mjs").TaskbarExperiment.isEnabledForLinux()};');assert result['identity']['sandbox']==6 and not result['identity']['disable']
 certPath=Path('/usr/local/share/ca-certificates/environment-proxy-ca.crt');certPem=certPath.read_text();certBase64=''.join(line for line in certPem.splitlines() if not line.startswith('-----'))
 result['browserProxyCaTrust']=m.script("const c=Cc['@mozilla.org/security/x509certdb;1'].getService(Ci.nsIX509CertDB).addCertFromBase64(arguments[0],'C,,');return {subject:c.subjectName,issuer:c.issuerName,sha256Fingerprint:c.sha256Fingerprint};",[certBase64]);result['browserProxyCaTrust']['source']='existing OS-trusted configured environment-proxy-ca.crt, issuer matched to verified host TLS peers; dedicated profile only'
 for label,url in [('vtmgo','https://www.vtmgo.be/'),('vrtmax','https://www.vrt.be/vrtmax/')]:
  row={'label':label,'url':url,'actualSite':True};result['sites'].append(row)
  m.call('Marionette:SetContext',{'value':'content'})
  try:m.call('WebDriver:Navigate',{'url':url})
  except Exception as ex:row['navigationError']=str(ex)
  time.sleep(3)
  try:row['content']=m.script('return {href:location.href,documentURI:document.documentURI,title:document.title,text:document.body?.innerText.slice(0,1800),ready:document.readyState,manifest:document.querySelector("link[rel=manifest]")?.href,icons:Array.from(document.querySelectorAll("link[rel*=icon]"),e=>({rel:e.rel,href:e.href,sizes:e.sizes?.value})),secure:window.isSecureContext};')
  except Exception as ex:row['contentError']=str(ex)
  m.call('Marionette:SetContext',{'value':'chrome'});row['product']=m.script(SNAPSHOT)
  try:(D/(label+'.png')).write_bytes(base64.b64decode(m.call('WebDriver:TakeScreenshot',{'full':False})['value']))
  except Exception as ex:row['screenshotError']=str(ex)
  content=row.get('content',{});good=content.get('secure') and not content.get('documentURI','').startswith('about:') and bool(content.get('manifest'))
  row['eligibleForActualSiteInstall']=bool(good)
  if good:
   # Install only the actual site through its genuine visible packaged UI.
   row['storeBefore']=m.async_script(STORE)
   m.click('#ssbPageAction');time.sleep(1);m.click('#ssb-app-install-button');time.sleep(5)
   row['storeAfter']=m.async_script(STORE);row['afterInstall']=m.script(SNAPSHOT)
  (D/'result.json').write_text(json.dumps(result,indent=2)+'\n')
 result['completed']=True
except Exception as ex:result['error']=str(ex);result['traceback']=traceback.format_exc()
finally:
 if m:
  try:
   m.call('Marionette:SetContext',{'value':'chrome'});result['normalQuit']=m.call('Marionette:Quit',{'flags':['eAttemptQuit']});result['exitCode']=proc.wait(40)
  except Exception as ex:result['quitError']=str(ex)
  m.socket.close()
 if proc and proc.poll() is None:proc.terminate();proc.wait(15);result['forcedTermination']=True
 result['productUnchanged']=before==inventory();result['ownedBrowserRemaining']=bool(proc and proc.poll() is None);(D/'product-hashes-after.json').write_text(json.dumps(inventory(),indent=2));(D/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
