import sys,json,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent));from run_steps import step
snapshot=Path('/workspace/qa-linux-additional/KAN-24/product-baseline/snapshot.js').read_text()
step('38-restart-render',context='chrome',script=snapshot)
step('39-native-route',script="location.href='about:preferences#tabsBrowsing';return true;")
time.sleep(3)
step('40-native-on-shadow',clickShadow="return document.getElementById('browserLayoutVerticalTabs').shadowRoot.querySelector('button')",script="return {horizontal:document.getElementById('browserLayoutHorizontalTabs').checked,vertical:document.getElementById('browserLayoutVerticalTabs').checked}",screenshot='40-native-on-shadow.png')
step('41-native-on-render',context='chrome',script=snapshot)
step('42-native-on-hub',script="location.href='about:hub#/features/design';return true;")
time.sleep(2)
step('43-native-on-hub-state',script="return {design:document.querySelector('input[name=design]:checked')?.value,style:document.querySelector('input[name=style]:checked')?.value}",screenshot='43-native-on-hub-state.png')
step('44-newwindow-menu',context='chrome',click='#PanelUI-menu-button',script="return Array.from(document.querySelectorAll('#appMenu-mainView toolbarbutton')).map(e=>({id:e.id,label:e.getAttribute('label'),r:e.getBoundingClientRect().toJSON()})).filter(e=>/window/i.test(e.id));")
