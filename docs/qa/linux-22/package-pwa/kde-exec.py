#!/usr/bin/env python3
import os,sys
from pathlib import Path
b=Path('/workspace/qa-linux-22/package-pwa')
env=dict(os.environ)
for entry in Path('/proc/18517/environ').read_bytes().split(b'\0'):
    if entry.startswith(b'DBUS_SESSION_BUS_ADDRESS='):
        env['DBUS_SESSION_BUS_ADDRESS']=entry.split(b'=',1)[1].decode()
env.update(HOME=str(b/'kde-home'),DISPLAY=':88',WAYLAND_DISPLAY='qa-kde-wayland',XDG_RUNTIME_DIR=str(b/'runtime'),QT_QPA_PLATFORM='wayland',XDG_DATA_DIRS=str(b/'sysroot/usr/share')+':/usr/share',XDG_CONFIG_DIRS=str(b/'sysroot/etc/xdg'),QT_PLUGIN_PATH=str(b/'sysroot/usr/lib/x86_64-linux-gnu/qt6/plugins'),QML2_IMPORT_PATH=str(b/'sysroot/usr/lib/x86_64-linux-gnu/qt6/qml'),LD_LIBRARY_PATH=str(b/'sysroot/usr/lib/x86_64-linux-gnu'),PATH=str(b/'sysroot/usr/bin')+':'+env['PATH'])
os.execvpe(sys.argv[1],sys.argv[1:],env)
