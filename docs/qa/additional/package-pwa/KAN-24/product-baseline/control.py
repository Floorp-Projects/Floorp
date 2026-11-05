import json,time,sys
from pathlib import Path
r=Path(__file__).parent
req=json.load(sys.stdin); out=r/req['out']
if out.exists():out.unlink()
(r/'command.json').write_text(json.dumps(req))
for i in range(180):
 if out.exists():print(out.read_text());break
 time.sleep(.2)
else:raise TimeoutError(req)
