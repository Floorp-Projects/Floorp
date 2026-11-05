#!/usr/bin/env python3
"""One actual YouTube Music safe-mode request, strict TLS and normal proxy."""
from pathlib import Path
import os,sys,socket,json,time,subprocess,hashlib,urllib.parse,ssl,traceback,base64,re
BASE=Path('/workspace/qa-linux-22/package-pwa');ROOT=Path(__file__).resolve().parent;SYS=BASE/'sysroot';PRODUCT=Path('/workspace/qa-linux-additional/KAN-24/followup/products/baseline/floorp');sys.path.insert(0,str(BASE));from pwa_ui_probe import Marionette,STORE
class StrictMarionette(Marionette):
 def __init__(self):
  self.socket=socket.create_connection(('127.0.0.1',2922),timeout=30);self.socket.settimeout(60);self.sequence=0;self.hello=self.receive();self.session=self.call('WebDriver:NewSession',{'capabilities':{'alwaysMatch':{'acceptInsecureCerts':False}}});self.call('Marionette:SetContext',{'value':'chrome'})
def cleanurl(s):
 try:
  p=urllib.parse.urlsplit(s);return urllib.parse.urlunsplit((p.scheme,p.netloc,p.path,'',''))
 except Exception:return '[unparseable url]'
def clean(v):
 if isinstance(v,dict):return {k:clean(x) for k,x in v.items()}
 if isinstance(v,list):return [clean(x) for x in v]
 if isinstance(v,str):return re.sub(r'(?:https?://|about:)[^\s"<>]+',lambda m:cleanurl(m.group(0)),v)
 return v
def save(n,v):(ROOT/(n+'.json')).write_text(json.dumps(clean(v),ensure_ascii=False,indent=2));return v
run=ROOT/'run';run.mkdir();home=run/'home';home.mkdir();profile=run/'profile';profile.mkdir();runtime=run/'runtime';runtime.mkdir(mode=0o700)
proxy=urllib.parse.urlsplit(os.environ.get('HTTPS_PROXY',os.environ.get('HTTP_PROXY','')));assert proxy.hostname,'normal managedproxy not configured'
prefs=['user_pref("marionette.port",2922);','user_pref("network.proxy.type",1);',f'user_pref("network.proxy.http",{json.dumps(proxy.hostname)});',f'user_pref("network.proxy.ssl",{json.dumps(proxy.hostname)});',f'user_pref("network.proxy.http_port",{proxy.port or 8080});',f'user_pref("network.proxy.ssl_port",{proxy.port or 8080});','user_pref("network.proxy.no_proxies_on","localhost, 127.0.0.1");'];(profile/'user.js').write_text('\n'.join(prefs)+'\n')
env=dict(os.environ,HOME=str(home),XDG_CONFIG_HOME=str(home/'.config'),XDG_DATA_HOME=str(home/'.local/share'),XDG_CACHE_HOME=str(home/'.cache'),XDG_RUNTIME_DIR=str(runtime),DISPLAY=':103',GDK_BACKEND='x11',MOZ_ENABLE_WAYLAND='0',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',LD_LIBRARY_PATH=str(SYS/'usr/lib/x86_64-linux-gnu'))
for k in list(env):
 if k.startswith('MOZ_DISABLE_') or k=='WAYLAND_DISPLAY':env.pop(k)
def inventory():return {str(p.relative_to(PRODUCT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in PRODUCT.rglob('*') if p.is_file()}
owned=[];m=None;browser=None;before=inventory();save('product-hashes-before',before);result={'sourceSha':'29a3eb23145da90706d8a94441373278521cc737','actualRequestedSite':'https://music.youtube.com/','proxy':'inherited normal managedproxy via dedicated-profile standard prefs','acceptInsecureCerts':False,'noAuthConsentPlaybackOrProxyBypass':True,'normalQuits':[]}
def start(n,cmd):
 f=(ROOT/(n+'.log')).open('w');p=subprocess.Popen(cmd,env=env,stdout=f,stderr=subprocess.STDOUT);owned.append((p,f));return p
try:
 assert not Path('/tmp/.X103-lock').exists() and not Path('/tmp/.X11-unix/X103').exists();x=start('xvfb',[str(SYS/'usr/bin/Xvfb'),':103','-screen','0','1280x900x24','-nolisten','tcp'])
 for _ in range(60):
  if Path('/tmp/.X11-unix/X103').exists():break
  assert x.poll() is None;time.sleep(.2)
 browser=start('browser',[str(PRODUCT/'floorp'),'--no-remote','--profile',str(profile),'--safe-mode','--marionette','about:blank'])
 for _ in range(180):
  try:m=StrictMarionette();break
  except OSError:time.sleep(.25)
 assert m,'Marionette unavailable';m.call('WebDriver:SetTimeouts',{'pageLoad':15000,'script':10000});result['explicitSessionCapabilities']=m.session.get('capabilities',{})
 result['identity']=save('identity',m.script("return {inSafeMode:Services.appinfo.inSafeMode,version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,profile:Services.dirsvc.get('ProfD',Ci.nsIFile).path,sandbox:Services.prefs.getIntPref('security.sandbox.content.level'),disabled:Services.env.exists('MOZ_DISABLE_CONTENT_SANDBOX')};"));assert result['identity']['inSafeMode'] and result['identity']['sandbox']==6 and not result['identity']['disabled'];assert result['identity']['profile']==str(profile)
 pem=Path('/usr/local/share/ca-certificates/environment-proxy-ca.crt').read_text();der=ssl.PEM_cert_to_DER_cert(pem);fp=hashlib.sha256(der).hexdigest();assert any(hashlib.sha256(c).hexdigest()==fp for c in ssl.create_default_context().get_ca_certs(binary_form=True)),'proxyCA not in normalOSdefaulttrust'
 cert64=''.join(x for x in pem.splitlines() if not x.startswith('-----'));ca=m.script("const c=Cc['@mozilla.org/security/x509certdb;1'].getService(Ci.nsIX509CertDB).addCertFromBase64(arguments[0],'C,,');return {subject:c.subjectName,issuer:c.issuerName,sha256Fingerprint:c.sha256Fingerprint};",[cert64]);result['normalProxyCaTrust']={**ca,'osDefaultTrustContainsSameCa':True,'source':'existing OS-trusted environment-proxy-ca.crt, exact fingerprint checked; dedicatedNSSprofile only; no certDB/PEM publication'}
 begin=time.monotonic();m.call('Marionette:SetContext',{'value':'content'})
 try:m.call('WebDriver:Navigate',{'url':'https://music.youtube.com/'})
 except Exception as e:result['navigationError']=str(e)
 time.sleep(2)
 result['content']=save('content',m.script("const strip=s=>{try{const u=new URL(s);return {url:u.protocol+'//'+u.host+u.pathname,queryPresent:!!u.search}}catch{return {url:s.split('?')[0],queryPresent:s.includes('?')}}};return {location:strip(location.href),documentURI:strip(document.documentURI),title:document.title,text:document.body?.innerText.slice(0,1800),ready:document.readyState,isSecureContext,crossOriginIsolated,manifest:document.querySelector('link[rel=manifest]')?strip(document.querySelector('link[rel=manifest]').href):null};"));m.call('Marionette:SetContext',{'value':'chrome'})
 result['browser']=save('browser-state',m.script("const b=gBrowser.selectedBrowser;let actor;try{actor=!!b.browsingContext.currentWindowGlobal.getActor('NRProgressiveWebApp')}catch(e){actor=String(e)}return {inSafeMode:Services.appinfo.inSafeMode,url:b.currentURI.spec,remoteType:b.remoteType,actor,securityState:b.securityUI?.state,isErrorPage:b.browsingContext.currentWindowGlobal?.documentURI?.spec?.startsWith('about:'),installButton:!!document.getElementById('ssbPageAction')?.getBoundingClientRect().width};"));(ROOT/'site.png').write_bytes(base64.b64decode(m.call('WebDriver:TakeScreenshot',{'full':False})['value']))
 c=result['content'];blocked=c['documentURI']['url'].startswith('about:') or c['location']['url'].startswith('about:') or 'consent.' in c['location']['url'] or bool(re.search('before you continue|not available in your|proxy server|unable to connect|access denied',c.get('text',''),re.I));result['blockedOrErrorPage']=blocked
 if not blocked and c['isSecureContext'] and c['manifest'] and result['browser']['installButton']:
  result['storeBefore']=m.async_script(STORE);m.click('#ssbPageAction');time.sleep(1);m.click('#ssb-app-install-button');time.sleep(5);result['storeAfter']=m.async_script(STORE);result['actualSiteUiInstallAttempted']=True
 else:result['actualSiteUiInstallAttempted']=False;result['stopReason']='actual page/network/consent barrier retained; no alternativehost, playback, auth or consent action'
 result['siteElapsedSeconds']=round(time.monotonic()-begin,2);result['completed']=True
except Exception as e:result['completed']=False;result['error']=str(e);result['traceback']=traceback.format_exc()
finally:
 if m:
  try:q=m.call('Marionette:Quit',{'flags':['eAttemptQuit']});m.socket.close();rc=browser.wait(timeout=40);result['normalQuits'].append({'response':q,'exitCode':rc});m=None
  except Exception as e:result['quitError']=str(e)
 for p,f in reversed(owned):
  if p.poll() is None:
   p.terminate()
   try:p.wait(timeout=10)
   except subprocess.TimeoutExpired:p.kill();p.wait()
  f.close()
 after=inventory();save('product-hashes-after',after);result['productUnchanged']=before==after;result['ownedProcessesRemaining']=sum(p.poll() is None for p,f in owned);result['displaySocketAbsent']=not Path('/tmp/.X11-unix/X103').exists();save('result',result);print(json.dumps(clean({k:result.get(k) for k in ['completed','error','blockedOrErrorPage','actualSiteUiInstallAttempted','siteElapsedSeconds','productUnchanged','ownedProcessesRemaining']})))
