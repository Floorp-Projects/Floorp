import json,time,sys
from pathlib import Path
r=Path(sys.argv[1])
req=json.load(sys.stdin); out=r/req['out']
if out.exists():out.unlink()
(r/'command.json').write_text(json.dumps(req))
for i in range(180):
 if out.exists():print(out.read_text());break
 time.sleep(.2)
else:raise TimeoutError(req)
