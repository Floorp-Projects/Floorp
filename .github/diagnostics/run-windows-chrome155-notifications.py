#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Genuine pinned Windows Chrome DOC notification observations; no OS settings changes."""
import argparse
import ctypes
import hashlib
import importlib.util
import json
import os
import re
import struct
from pathlib import Path, PurePosixPath
import shutil
import stat
import subprocess
import sys
import time
import urllib.request
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
VERSION = '155.0.8059.84'
REVISION = '@5869fe313571d75152b77813cf0a4ee26a4e6df0'
VENDOR_METADATA = 'https://googlechromelabs.github.io/chrome-for-testing/155.0.8059.84.json'
METADATA_SHA = 'b3e9c2d6fd21794f210fec79dbfe5813389fd36ab5919e6a1f99dbe0f5ab310a'
VENDOR_ARCHIVE = 'https://storage.googleapis.com/chrome-for-testing-public/155.0.8059.84/win64/chrome-win64.zip'
PINNED_OFFICIAL_ARCHIVE_BYTES = 206114888
PINNED_OFFICIAL_ARCHIVE_SHA256 = '507f2a031441d10c62b5e3d4e863f09ac1198a571c1419976cbd8387f639afde'
HELPERS = {'run-windows-old-diagnostic.py':'2908ab5a2368c1ac38b84031be502abe0fa16dbb7bec314fd8d3d893549526c0',
           'observe-windows-alert-state.py':'aa4da4e360459d540fe3516c7e56b64e34d43c19626a4d4b9ee5049aef055997'}


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1048576), b''): digest.update(block)
    return digest.hexdigest()


def record(path):
    return {'path': str(path.resolve()), 'bytes': path.stat().st_size, 'sha256': sha(path)}


def save(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def checked_helpers():
    for name, expected in HELPERS.items():
        if sha(HERE / name) != expected: raise RuntimeError('Reviewed helper changed: ' + name)
    return load('owned_chrome_job', HERE / 'run-windows-old-diagnostic.py')


def observe(args):
    if sys.platform != 'win32': raise RuntimeError('Actual Windows native APIs required')
    checked_helpers()
    original = load('original_readonly_os_state', HERE / 'observe-windows-alert-state.py')
    result = original.observe()
    result['scope'] = 'External readonly Windows state while owned Chrome may be alive; not an in-Chrome SHQuery call'
    user = ctypes.WinDLL('user32', use_last_error=True)
    LONG, DWORD, HANDLE, BOOL = ctypes.c_int32, ctypes.c_uint32, ctypes.c_void_p, ctypes.c_int32
    class RECT(ctypes.Structure): _fields_ = [('left',LONG),('top',LONG),('right',LONG),('bottom',LONG)]
    class MONITORINFO(ctypes.Structure): _fields_ = [('cbSize',DWORD),('monitor',RECT),('work',RECT),('flags',DWORD)]
    if ctypes.sizeof(RECT) != 16 or ctypes.sizeof(MONITORINFO) != 40: raise RuntimeError('Invalid LLP64 layout')
    callback = ctypes.WINFUNCTYPE(BOOL,HANDLE,ctypes.c_ssize_t)
    declarations = [('EnumWindows',[callback,ctypes.c_ssize_t],BOOL),
       ('GetWindowThreadProcessId',[HANDLE,ctypes.POINTER(DWORD)],DWORD),
       ('GetWindowRect',[HANDLE,ctypes.POINTER(RECT)],BOOL),('GetForegroundWindow',[],HANDLE),
       ('MonitorFromWindow',[HANDLE,DWORD],HANDLE),('GetMonitorInfoW',[HANDLE,ctypes.POINTER(MONITORINFO)],BOOL),
       ('IsWindowVisible',[HANDLE],BOOL),('IsZoomed',[HANDLE],BOOL),('IsIconic',[HANDLE],BOOL),('GetDpiForWindow',[HANDLE],DWORD)]
    for name, parameters, returns in declarations:
        function=getattr(user,name); function.argtypes=parameters; function.restype=returns
    def rectangle(value): return {key:int(getattr(value,key)) for key in ['left','top','right','bottom']}
    windows=[]
    def visit(hwnd, unused):
        process=DWORD();thread=user.GetWindowThreadProcessId(hwnd,ctypes.byref(process))
        if process.value != args.chrome_pid or not args.chrome_pid or not user.IsWindowVisible(hwnd): return True
        rect=RECT();info=MONITORINFO();info.cbSize=ctypes.sizeof(info);monitor=user.MonitorFromWindow(hwnd,2)
        windows.append({'hwnd':hex(hwnd),'processId':process.value,'threadId':thread,
          'bounds':rectangle(rect) if user.GetWindowRect(hwnd,ctypes.byref(rect)) else None,
          'workArea':rectangle(info.work) if monitor and user.GetMonitorInfoW(monitor,ctypes.byref(info)) else None,
          'maximized':bool(user.IsZoomed(hwnd)),'minimized':bool(user.IsIconic(hwnd)),
          'dpi':int(user.GetDpiForWindow(hwnd))})
        return True
    thunk=callback(visit)
    if not user.EnumWindows(thunk,0): raise ctypes.WinError(ctypes.get_last_error())
    foreground=user.GetForegroundWindow();pid=DWORD()
    thread=user.GetWindowThreadProcessId(foreground,ctypes.byref(pid)) if foreground else 0
    result.update({'ownedChromePid':args.chrome_pid,'ownedChromeWindows':windows,
      'foreground':{'hwnd':hex(foreground) if foreground else None,'processId':pid.value,'threadId':thread,
                    'belongsToOwnedChrome':bool(args.chrome_pid and pid.value==args.chrome_pid)}})
    # The only global UI search is exact authored notification text on this
    # dedicated account-free VM. No UI invoke, selection, focus or settings API.
    title = args.title.replace("'", "''")
    image = str(args.output.resolve().with_suffix('.png')).replace("'", "''")
    message = args.expected_message.replace("'", "''")
    script = r'''
$ErrorActionPreference='Stop';[Console]::OutputEncoding=[System.Text.UTF8Encoding]::new($false)
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
$condition=[System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::NameProperty,'TITLE')
$deadline=[DateTime]::UtcNow.AddMilliseconds(WAIT)
$rows=@(); $samples=0
try {
 do {
  $found=[System.Windows.Automation.AutomationElement]::RootElement.FindAll([System.Windows.Automation.TreeScope]::Descendants,$condition)
  $samples++
  $rows=@(foreach($element in $found){
   $c=$element.Current;$r=$c.BoundingRectangle
   $container=[System.Windows.Automation.TreeWalker]::ControlViewWalker.GetParent($element)
   $parent=if($container){[System.Windows.Automation.TreeWalker]::ControlViewWalker.GetParent($container)}else{$null}
   if($parent){$container=$parent}
   $texts=if($container){@($container.FindAll([System.Windows.Automation.TreeScope]::Descendants,[System.Windows.Automation.Condition]::TrueCondition)|Select-Object -First 50|ForEach-Object{$_.Current.Name})}else{@()}
   [ordered]@{name=$c.Name;processId=$c.ProcessId;className=$c.ClassName;frameworkId=$c.FrameworkId;isOffscreen=$c.IsOffscreen;nearbyText=$texts;bounds=@{left=$r.Left;top=$r.Top;width=$r.Width;height=$r.Height}}
  })
  if(@($rows|Where-Object{-not $_.isOffscreen -and $_.bounds.width -gt 0 -and $_.bounds.height -gt 0 -and (('MESSAGE' -eq '') -or ($_.nearbyText -contains 'MESSAGE'))}).Count -gt 0){break}
  if([DateTime]::UtcNow -lt $deadline){Start-Sleep -Milliseconds 250}
 }while([DateTime]::UtcNow -lt $deadline)
 $area=[System.Windows.Forms.SystemInformation]::VirtualScreen
 $bitmap=New-Object System.Drawing.Bitmap($area.Width,$area.Height)
 $graphics=[System.Drawing.Graphics]::FromImage($bitmap)
 try {$graphics.CopyFromScreen($area.Left,$area.Top,0,0,$area.Size);$bitmap.Save('IMAGE',[System.Drawing.Imaging.ImageFormat]::Png)}finally{$graphics.Dispose();$bitmap.Dispose()}
 [ordered]@{samples=$samples;matches=$rows;expectedMessage='MESSAGE';screenshot='IMAGE';settingsChanged=$false;uiActionsInvoked=$false}|ConvertTo-Json -Depth 10 -Compress
}catch{[ordered]@{error=$_.Exception.Message;samples=$samples;matches=$rows;settingsChanged=$false;uiActionsInvoked=$false}|ConvertTo-Json -Depth 10 -Compress;exit 1}
'''.replace('TITLE',title).replace('IMAGE',image).replace('WAIT',str(args.ui_wait_ms)).replace('MESSAGE',message)
    log=args.output.with_suffix('.ui.log');log.parent.mkdir(parents=True,exist_ok=True)
    try:
        child=subprocess.run(['pwsh.exe','-NoProfile','-NonInteractive','-Command',script],capture_output=True,
                             timeout=args.ui_wait_ms/1000+10)
    except subprocess.TimeoutExpired as error:
        log.write_bytes((error.stdout or b'')+(error.stderr or b''))
        result['authoredUi']={'error':'Bounded readonly UI observer timeout','observerExitCode':None,'uiActionsInvoked':False}
        save(args.output,result)
        return
    log.write_bytes(child.stdout+child.stderr)
    try: result['authoredUi']=json.loads(child.stdout.decode('utf-8-sig').strip().splitlines()[-1])
    except (UnicodeError,ValueError,IndexError): result['authoredUi']={'error':'UI observation did not emit valid JSON','exitCode':child.returncode}
    result['authoredUi']['observerExitCode']=child.returncode
    result['authoredUi']['limit']='Exact authored title observations/screenshots only; empty/error is not proof the native backend accepted or displayed a toast.'
    save(args.output,result)


def acquire(url,target,maximum):
    if url not in [VENDOR_METADATA, VENDOR_ARCHIVE]: raise RuntimeError('Exact official HTTPS destination required')
    if target.exists(): raise RuntimeError('Fresh vendor output required')
    started=time.monotonic();size=0;digest=hashlib.sha256()
    with urllib.request.urlopen(url,timeout=30) as response,target.open('xb') as stream:
        if response.status!=200: raise RuntimeError('Official vendor HTTP200 required')
        final_url = response.geturl()
        if final_url != url: raise RuntimeError('Exact canonical official HTTPS final URL required')
        headers={key:response.headers.get(key) for key in ['Content-Length','ETag','Last-Modified','Content-Type']}
        for block in iter(lambda:response.read(1048576),b''):
            if time.monotonic()-started>300: raise RuntimeError('Bounded official vendor delivery expired')
            size+=len(block)
            if size>maximum: raise RuntimeError('Official vendor body exceeds admitted size')
            digest.update(block);stream.write(block)
        if time.monotonic()-started>300: raise RuntimeError('Bounded official vendor delivery expired')
    if headers['Content-Length'] and int(headers['Content-Length'])!=size: raise RuntimeError('Incomplete official body')
    return {'url':url,'finalUrl':final_url,'bytes':size,'sha256':digest.hexdigest(),'headers':headers,'elapsedSeconds':time.monotonic()-started}


def extract(archive,destination):
    destination.mkdir(exist_ok=False);seen=set();total=0;members=[]
    with zipfile.ZipFile(archive) as source:
        for row in source.infolist():
            path=PurePosixPath(row.filename);parts=path.parts
            if (path.is_absolute() or not parts or '\\' in row.filename or ':' in row.filename or
                any(p in ['.','..'] or p.endswith((' ','.')) or p.split('.')[0].upper() in
                    {'CON','PRN','AUX','NUL',*[f'COM{i}' for i in range(1,10)],*[f'LPT{i}' for i in range(1,10)]} for p in parts)):
                raise RuntimeError('Unsafe actual vendor ZIP path')
            if row.filename.casefold() in seen: raise RuntimeError('Ambiguous actual vendor ZIP entry')
            seen.add(row.filename.casefold());total+=row.file_size
            if len(seen)>10000 or total>2147483648 or row.file_size>536870912 or row.flag_bits&1 or stat.S_IFMT(row.external_attr>>16) not in [0,stat.S_IFREG,stat.S_IFDIR]:
                raise RuntimeError('Unsupported or excessive actual vendor ZIP entry')
            target=destination/Path(*parts)
            if row.is_dir():target.mkdir(parents=True,exist_ok=True);continue
            target.parent.mkdir(parents=True,exist_ok=True)
            digest=hashlib.sha256();written=0
            with source.open(row) as current,target.open('xb') as out:
                for block in iter(lambda:current.read(1048576),b''):
                    written+=len(block)
                    if written>row.file_size: raise RuntimeError('Actual ZIP member exceeds declared size')
                    digest.update(block);out.write(block)
            if written!=row.file_size: raise RuntimeError('Actual ZIP member incomplete')
            members.append({'relativePath':path.as_posix(),'bytes':written,'sha256':digest.hexdigest()})
    return sorted(members,key=lambda row:row['relativePath'])


def require_official_archive(receipt):
    if (receipt.get('url')!=VENDOR_ARCHIVE or receipt.get('finalUrl')!=VENDOR_ARCHIVE or
        receipt.get('bytes')!=PINNED_OFFICIAL_ARCHIVE_BYTES or
        receipt.get('sha256')!=PINNED_OFFICIAL_ARCHIVE_SHA256):
        raise RuntimeError('Official unsigned CfT archive differs from both prior authenticated deliveries')


def require_physical_archive(receipt,path):
    actual=record(path)
    if actual['bytes']!=receipt['bytes'] or actual['sha256']!=receipt['sha256']:
        raise RuntimeError('Saved official archive differs from authenticated HTTPS body')
    return actual


def require_unsigned_cft_signature(signature):
    # CfT is a separate official unsigned test distribution, not installed Google Chrome.
    # Null certificate is retained as observed. Other missing/invalid observations reject.
    if (not all(key in signature for key in ['status','subject','thumbprint','productName','productVersion']) or
        signature.get('status')!='NotSigned' or signature.get('subject') is not None or
        signature.get('thumbprint') is not None or signature.get('productName')!='Google Chrome for Testing' or
        not re.match(r'^155\.0\.8059\.84(?:\s|$)',signature.get('productVersion',''))):
        raise RuntimeError('Exact unsigned official CfT signature/version observation required')


def require_extracted_inventory(members,actual):
    expected=sorted(members,key=lambda row:row['relativePath'])
    physical=sorted([{key:row[key] for key in ['relativePath','bytes','sha256']} for row in actual],
                    key=lambda row:row['relativePath'])
    if expected!=physical:
        raise RuntimeError('Complete extracted application differs from SHA-pinned ZIP members')
    executable=[row for row in expected if row['relativePath']=='chrome-win64/chrome.exe']
    if len(executable)!=1:raise RuntimeError('Exactly one SHA-bound official CfT executable required')
    return executable[0]


def inventory(root):
    result=[]
    for path in sorted(root.rglob('*')):
        if path.is_symlink():raise RuntimeError('Unexpected vendor application link')
        if path.is_dir():continue
        if not path.is_file():raise RuntimeError('Unexpected vendor application object')
        row=record(path);row['relativePath']=path.relative_to(root).as_posix();result.append(row)
    return result


def lifecycle(row):
    return row['exitCode']==0 and row['watchdogStopped'] is False and row['ownJobAssignedBeforeWork'] is True and row['remainingOwned']==0 and row['activeOwnedAtRunnerExit']==0


def main(args):
    if sys.platform!='win32':raise RuntimeError('Actual Windows required')
    old=checked_helpers();out=args.output.resolve();out.mkdir(exist_ok=False)
    app=args.application.resolve();vendor=args.vendor.resolve()
    roots=[out,app,vendor]
    if any(a==b or a.is_relative_to(b) or b.is_relative_to(a) for i,a in enumerate(roots) for b in roots[i+1:]):raise RuntimeError('Disjoint fresh roots required')
    environment=dict(os.environ)
    for name in list(environment):
        if name in ['GH_TOKEN','GITHUB_TOKEN','MOZ_HEADLESS','MOZ_DISABLE_CONTENT_SANDBOX','MOZ_DISABLE_GPU_SANDBOX','MOZ_DISABLE_RDD_SANDBOX','MOZ_DISABLE_GMP_SANDBOX'] or name.startswith('FLOORP_TEST_') or name.startswith('FLOORP_CANDIDATE_'):
            environment.pop(name,None)
    original=HERE.parents[1]/'browser-features/modules/modules/chrome-web-store/test/ChromeNotificationsCloseNative.test.mts'
    if sha(original)!='6a86399fba5653672f6d2bdd99adbc1cbebb5a7d5bbcaa9b0871a5eed3f06936':raise RuntimeError('Frozen original DOC source changed')
    producers=[record(original),record(Path(__file__)),record(HERE/'chrome155-notifications-reference.mjs'),record(HERE/'chrome155-notification-reference-types.ts')]+[record(HERE/name) for name in HELPERS]
    for row in producers:
        path=out/'producer-source'/Path(row['path']).name;path.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(row['path'],path)
    discovery_script = r"""
$ErrorActionPreference='Stop';[Console]::OutputEncoding=[System.Text.UTF8Encoding]::new($false)
$paths=@("$env:ProgramFiles\Google\Chrome\Application\chrome.exe","${env:ProgramFiles(x86)}\Google\Chrome\Application\chrome.exe","$env:LOCALAPPDATA\Google\Chrome\Application\chrome.exe")
$rows=@(foreach($path in $paths|Select-Object -Unique){if(Test-Path -LiteralPath $path -PathType Leaf){$s=Get-AuthenticodeSignature -LiteralPath $path;$v=(Get-Item -LiteralPath $path).VersionInfo;[ordered]@{path=(Get-Item -LiteralPath $path).FullName;status=[string]$s.Status;subject=$s.SignerCertificate.Subject;thumbprint=$s.SignerCertificate.Thumbprint;fileVersion=$v.FileVersion;productVersion=$v.ProductVersion;productName=$v.ProductName}}})
[ordered]@{candidates=$rows;settingsChanged=$false}|ConvertTo-Json -Depth 6 -Compress
"""
    discovery=old.bounded_windows(['pwsh.exe','-NoProfile','-NonInteractive','-Command',discovery_script],HERE,environment,out/'installed-discovery',seconds=45)
    if not lifecycle(discovery):raise RuntimeError('Owned readonly installed Chrome discovery failed')
    discovered=json.loads((out/'installed-discovery/supervisor.log').read_text(encoding='utf-8-sig').strip().splitlines()[-1])
    save(out/'installed-chrome-discovery.json',discovered)
    def valid_signature(value):
        return (value.get('status')=='Valid' and re.search(r'(?:^|,\s*)CN=Google LLC(?:,|$)',value.get('subject','')) and
                re.match(r'^155\.0\.8059\.84(?:\s|$)',value.get('productVersion','')))
    def win64(path):
        with path.open('rb') as stream:
            if stream.read(2)!=b'MZ':return False
            stream.seek(0x3c);offset=struct.unpack('<I',stream.read(4))[0]
            if offset>1048576:return False
            stream.seek(offset);header=stream.read(6)
            return header[:4]==b'PE\0\0' and struct.unpack('<H',header[4:])[0]==0x8664
    candidates=[row for row in discovered.get('candidates',[]) if valid_signature(row) and win64(Path(row['path']))]
    receipt=None;official_members=None;executable_binding=None
    if candidates:
        signature=candidates[0];chrome=Path(signature['path']).resolve(strict=True);app=chrome.parent
        distribution='existing installed Google-signed Chrome win64, exact static version; actual revision still must pass'
        distribution_type='installed-google-signed-chrome-win64'
    else:
        vendor.mkdir(exist_ok=False)
        meta=vendor/'metadata.json';metadata_receipt=acquire(VENDOR_METADATA,meta,65536)
        if metadata_receipt['sha256']!=METADATA_SHA:raise RuntimeError('Exact vendor version metadata differs')
        data=json.loads(meta.read_text());choices=[r for r in data['downloads']['chrome'] if r['platform']=='win64']
        if data['version']!=VERSION or len(choices)!=1 or choices[0]['url']!=VENDOR_ARCHIVE:raise RuntimeError('Exact official win64 vendor selection differs')
        receipt=acquire(VENDOR_ARCHIVE,vendor/'archive.zip',367001600)
        save(out/'official-vendor-delivery.json',{'metadata':metadata_receipt,'archive':receipt,
            'pinnedPriorWholeZip':{'bytes':PINNED_OFFICIAL_ARCHIVE_BYTES,'sha256':PINNED_OFFICIAL_ARCHIVE_SHA256,
              'authenticatedPriorRuns':[39352017818,39352493049]},
            'scope':'Official unsigned CfT: exact canonical HTTPS, fixed metadata and independently repeated wholeZIP hash, derived member hashes plus physical equality, actual NotSigned, hard runtime gate. No Google signature or toast-readiness assumption.'})
        require_official_archive(receipt)
        physical_archive=require_physical_archive(receipt,vendor/'archive.zip')
        save(out/'official-saved-archive-binding.json',physical_archive)
        official_members=extract(vendor/'archive.zip',app)
        save(out/'official-zip-member-inventory.json',official_members)
        chrome=app/'chrome-win64/chrome.exe'
        if not chrome.is_file() or not win64(chrome):raise RuntimeError('Official Chrome win64 executable absent')
        quoted=str(chrome).replace("'","''")
        script="$ErrorActionPreference='Stop';[Console]::OutputEncoding=[System.Text.UTF8Encoding]::new($false);$s=Get-AuthenticodeSignature -LiteralPath '"+quoted+"';$v=(Get-Item -LiteralPath '"+quoted+"').VersionInfo;[ordered]@{status=[string]$s.Status;subject=$s.SignerCertificate.Subject;thumbprint=$s.SignerCertificate.Thumbprint;fileVersion=$v.FileVersion;productVersion=$v.ProductVersion;productName=$v.ProductName}|ConvertTo-Json -Compress"
        signed=old.bounded_windows(['pwsh.exe','-NoProfile','-NonInteractive','-Command',script],HERE,environment,out/'authenticode',seconds=30)
        if not lifecycle(signed):raise RuntimeError('Owned native Authenticode observation failed')
        signature=json.loads((out/'authenticode/supervisor.log').read_text(encoding='utf-8-sig').strip().splitlines()[-1])
        save(out/'actual-cft-authenticode-observation.json',signature)
        require_unsigned_cft_signature(signature)
        distribution='official unsigned Chrome for Testing win64; native toast activation not assumed'
        distribution_type='official-unsigned-chrome-for-testing-win64'
    before=inventory(app);save(out/'application-inventory-before.json',before)
    if official_members is not None:
        member=require_extracted_inventory(official_members,before)
        if sha(chrome)!=member['sha256']:raise RuntimeError('Actual executable differs from verified ZIP member')
        executable_binding={'expectedSha256':member['sha256'],'expectedBytes':member['bytes'],
          'actualExecutable':record(chrome),'relativePath':member['relativePath'],
          'derivedFromVerifiedWholeZipSha256':PINNED_OFFICIAL_ARCHIVE_SHA256,
          'derivedFromVerifiedWholeZipBytes':PINNED_OFFICIAL_ARCHIVE_BYTES,
          'completeInventoryPhysicalEquality':True,'priorReceiptsContainedExeDigest':False,
          'provenance':'Expected EXE hash is derived from the exact immutable wholeZIP matched in both prior actual deliveries; never attributed to an absent prior EXE receipt.'}
        save(out/'official-executable-binding.json',executable_binding)
    authorization={'chrome':record(chrome),'signature':signature,'version':VERSION,'revision':REVISION,'distribution':distribution,'distributionType':distribution_type,'archive':receipt,'executableBinding':executable_binding,'nativeToastReadinessAssumed':False}
    save(out/'authorization.json',authorization)
    summary={'scope':'Actual Windows Chrome DOC public observation only; zero qualification credit','producerFiles':producers,'authorization':authorization,'nativeToastReadinessAssumed':False,'installedDiscovery':discovered,'cases':[],'observationCompleted':False}
    save(out/'summary.json',summary)
    for geometry in ['normal','maximized']:
        case=out/'cases'/geometry;case.parent.mkdir(exist_ok=True)
        command=['node',HERE/'chrome155-notifications-reference.mjs','--chrome',chrome,'--output',case,'--geometry',geometry,'--python',sys.executable,'--observer',Path(__file__).resolve(),'--authorization',out/'authorization.json']
        owned=old.bounded_windows(command,HERE,environment,out/('owned-'+geometry),seconds=300)
        result=json.loads((case/'result.json').read_text(encoding='utf-8')) if (case/'result.json').is_file() else None
        summary['cases'].append({'geometry':geometry,'owned':owned,'result':result});save(out/'summary.json',summary)
        if not lifecycle(owned) or not result or not result['observationCompleted']:break
    after=inventory(app);save(out/'application-inventory-after.json',after)
    summary['applicationUnchanged']=before==after
    for row in producers:
        if sha(Path(row['path']))!=row['sha256']:raise RuntimeError('Reference source changed during observation')
    summary['observationCompleted']=len(summary['cases'])==2 and summary['applicationUnchanged'] and all(lifecycle(r['owned']) and r['result']['observationCompleted'] for r in summary['cases'])
    summary['vendorPayloadUploaded']=False
    summary['limitation']='Public calls, actual UI/text pixels and native-ready histograms are separate observations. No native toast or Floorp compatibility PASS follows solely from completion.'
    save(out/'summary.json',summary)
    return 0 if summary['observationCompleted'] else 1


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snapshot',action='store_true');parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--expected-message',default='');parser.add_argument('--chrome-pid',type=int,default=0);parser.add_argument('--title',default='Floorp authored native programmatic close');parser.add_argument('--ui-wait-ms',type=int,default=0)
    parser.add_argument('--application',type=Path);parser.add_argument('--vendor',type=Path)
    args=parser.parse_args()
    if not 0<=args.ui_wait_ms<=20000:parser.error('Bounded actual UI observation required')
    if args.snapshot:observe(args)
    else:
        if not args.application or not args.vendor:parser.error('--application and --vendor required')
        raise SystemExit(main(args))
