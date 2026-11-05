#!/bin/sh
# Read-only KWin observation. The caller must supply the same nested KDE bus
# and runtime environment used for the actual product, e.g. via kde-exec.py.
set -eu
qa_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
qa_script_name="qa-kan48-observe-$$"
qa_qdbus=${QA_QDBUS:-qdbus6}
qa_script_id=$("$qa_qdbus" org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript "$qa_dir/window-snapshot.js" "$qa_script_name")
"$qa_qdbus" org.kde.KWin "/Scripting/Script$qa_script_id" org.kde.kwin.Script.run
"$qa_qdbus" org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript "$qa_script_name"
echo 'Read QA_KAN48_WINDOWS lines from the nested KWin log; getWindowInfo accepts each internalId.'
