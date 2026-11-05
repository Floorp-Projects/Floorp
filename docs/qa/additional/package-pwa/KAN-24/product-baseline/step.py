import sys,json,time,base64
from pathlib import Path
sys.path.insert(0,'/workspace/qa-linux-22/package-pwa')
from pwa_ui_probe import Marionette
r=Path(__file__).parent
m=Marionette('127.0.0.1',2910)
try:
 m.call('Marionette:SetContext',{'value':sys.argv[1]})
 result=m.script(sys.stdin.read())
 print(json.dumps(result,indent=2))
finally:
 m.call('WebDriver:DeleteSession');m.socket.close()
