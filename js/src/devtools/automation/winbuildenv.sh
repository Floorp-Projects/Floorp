mk_export_correct_style() {
  echo "export $1=$(cmd.exe //c echo %$1%)"
}

topsrcdir="$SOURCE"

# Tooltool installs in parent of topsrcdir for spidermonkey builds.
# Resolve that path since the mozconfigs assume tooltool installs in
# topsrcdir.
export VSPATH="$(cd ${topsrcdir}/.. && pwd)/vs2017_15.8.4"

if [ -n "$USE_64BIT" ]; then
  . $topsrcdir/build/win64/mozconfig.vs-latest
else
  . $topsrcdir/build/win32/mozconfig.vs-latest
fi

mk_export_correct_style CC
mk_export_correct_style CXX
mk_export_correct_style LINKER
mk_export_correct_style WINDOWSSDKDIR
mk_export_correct_style DIA_SDK_PATH
mk_export_correct_style VC_PATH

# PATH also needs to point to mozmake.exe, which can come from either
# newer mozilla-build or tooltool.
if ! which mozmake 2>/dev/null; then
    export PATH="$PATH:$SOURCE/.."
    if ! which mozmake 2>/dev/null; then
  ( cd $SOURCE/..; $SOURCE/mach artifact toolchain -v --tooltool-manifest $SOURCE/browser/config/tooltool-manifests/${platform:-win32}/releng.manifest --retry 4${TOOLTOOL_CACHE:+ --cache-dir ${TOOLTOOL_CACHE}}${MOZ_TOOLCHAINS:+ ${MOZ_TOOLCHAINS}})
    fi
fi
