#!/usr/bin/env python3
"""Run unchanged PR3030's standard frozen frontend production pre-mach phase."""
import subprocess,os,json,hashlib,time
from pathlib import Path
OUT=Path(__file__).resolve().parent;WORK=Path('/workspace/qa-linux-full-build/floorp3030');DENO=Path('/workspace/.floorp-cloud/tools/node_modules/.bin/deno');EXPECTED='e55152c559a141001a375ac77098d141b0db3ca7'
def git(*args):return subprocess.check_output(['git','-C',str(WORK),*args],text=True).strip()
assert git('rev-parse','HEAD')==EXPECTED
assert git('status','--porcelain')=='','initial checkout must be clean'
env=dict(os.environ,HOME='/workspace/qa-linux-full-build/home3030',XDG_CONFIG_HOME='/workspace/qa-linux-full-build/home3030/.config',XDG_CACHE_HOME='/workspace/qa-linux-full-build/home3030/.cache',XDG_DATA_HOME='/workspace/qa-linux-full-build/home3030/.local/share',DENO_DIR='/workspace/.floorp-cloud/deno-cache')
env['PATH']=str(DENO.parent)+':/workspace/.floorp-cloud/tools/node_modules/node/bin:'+env.get('PATH','')
env.pop('MOZ_DISABLE_CONTENT_SANDBOX',None)
lock=WORK/'floorp-runtime.lock.json';sourceLock=json.loads(lock.read_text())
record={'pr':'https://github.com/Floorp-Projects/Floorp/pull/3030','sourceSha':EXPECTED,'runtimeCommit':sourceLock['source']['commit'],'runtimeLockSha256':hashlib.sha256(lock.read_bytes()).hexdigest(),'worktree':str(WORK),'fullProductCompleted':False,'scope':'standard frontend before-mach phase only; mach configure/build/package is separate','sandboxDisableEnvPresent':False,'stages':[]}
record['denoVersion']=subprocess.check_output([str(DENO),'--version'],env=env,text=True)
(OUT/'build-record.json').write_text(json.dumps(record,indent=2))
for name,command,production in [('frozen-dependencies',[str(DENO),'install','--frozen'],False),('before-mach',[str(DENO),'task','feles-build','build','--phase','before-mach'],True)]:
 stageenv=dict(env)
 if production:stageenv['NODE_ENV']='production'
 start=time.time()
 with (OUT/(name+'.log')).open('w') as log:r=subprocess.run(command,cwd=WORK,env=stageenv,stdout=log,stderr=subprocess.STDOUT)
 stage={'name':name,'command':command,'nodeEnv':'production' if production else 'not overridden','exitCode':r.returncode,'elapsedSeconds':round(time.time()-start,2)};record['stages'].append(stage)
 record['trackedDiffAfter']=git('diff','--stat');record['runtimeLockUnchanged']=hashlib.sha256(lock.read_bytes()).hexdigest()==record['runtimeLockSha256']
 (OUT/'build-record.json').write_text(json.dumps(record,indent=2));print(json.dumps(stage),flush=True)
 if r.returncode:raise SystemExit(r.returncode)
record['frontendBeforeMachCompleted']=True
record['trackedStatusAfter']=git('status','--porcelain')
(OUT/'build-record.json').write_text(json.dumps(record,indent=2))
print('standard frontend before-mach complete; native configure/build/package not yet run',flush=True)
