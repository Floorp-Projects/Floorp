#!/usr/bin/env python3
"""Execute standard Flatpak tools in the dedicated candidate user installation."""
from pathlib import Path
import os
import sys
b = Path(__file__).resolve().parents[1]
home = b / 'KAN-8/full-build-home'
home.mkdir(exist_ok=True)
env = dict(os.environ, HOME=str(home), XDG_DATA_HOME=str(home / '.local/share'),
    XDG_CONFIG_HOME=str(home / '.config'), XDG_CACHE_HOME=str(home / '.cache'),
    XDG_RUNTIME_DIR=str(b / 'runtime'),
    DBUS_SYSTEM_BUS_ADDRESS='unix:path=' + str(b / 'KAN-8/system-probe-bus'),
    FLATPAK_BWRAP='/usr/bin/bwrap',
    FLATPAK_DBUSPROXY=str(b / 'sysroot/usr/bin/xdg-dbus-proxy'),
    FLATPAK_VALIDATE_ICON=str(b / 'sysroot/usr/libexec/flatpak-validate-icon'),
    FLATPAK_BUILDER_DEBUGEDIT=str(b / 'sysroot/usr/bin/debugedit'),
    LD_LIBRARY_PATH=str(b / 'sysroot/usr/lib/x86_64-linux-gnu'),
    PATH=str(b / 'KAN-8/build-tool-bin') + ':' + str(b / 'sysroot/usr/bin') + ':/usr/bin:/bin')
env.pop('MOZ_DISABLE_CONTENT_SANDBOX', None)
os.execvpe(sys.argv[1], sys.argv[1:], env)
