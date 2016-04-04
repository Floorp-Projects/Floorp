# We will be sourcing mozconfig files, which end up calling mk_add_options with
# various settings. We only need the variable settings they create along the
# way.
mk_add_options() {
  : do nothing
}

topsrcdir="$SOURCE"

# Tooltool installs in parent of topsrcdir for spidermonkey builds.
# Resolve that path since the mozconfigs assume tooltool installs in
# topsrcdir.
VSPATH="$(cd ${topsrcdir}/.. && pwd)/vs2015u1"

# When running on a developer machine, several variables will already
# have the right settings and we will need to keep them since the
# Windows mozconfigs overwrite them.
export OLD_INCLUDE=$(IFS=';'; for d in $INCLUDE; do ( cd "$d" && echo -n $(pwd): ); done)
export OLD_LIB=$(IFS=';'; for d in $LIB; do ( cd "$d" && echo -n $(pwd): ); done)
export OLD_LIBPATH=$(IFS=';'; for d in $LIBPATH; do ( cd "$d" && echo -n $(pwd): ); done)

# The various browser/config/mozconfigs/win32/* files use these checks to pick
# the compiler.
if $USE_64BIT; then
  . $topsrcdir/build/win64/mozconfig.vs2015
elif test "$PROCESSOR_ARCHITECTURE" = "AMD64" -o "$PROCESSOR_ARCHITEW6432" = "AMD64"; then
  . $topsrcdir/build/win32/mozconfig.vs2015-win64
else
  . $topsrcdir/build/win32/mozconfig.vs2010
fi

# PATH also needs to point to mozmake.exe, which can come from either
# newer mozilla-build or tooltool.
if ! which mozmake 2>/dev/null; then
    export PATH="$PATH:$SOURCE/.."
    if ! which mozmake 2>/dev/null; then
	TT_SERVER=${TT_SERVER:-https://api.pub.build.mozilla.org/tooltool/}
	( cd $SOURCE/..; ./scripts/scripts/tooltool/tooltool_wrapper.sh $SOURCE/browser/config/tooltool-manifests/${platform:-win32}/releng.manifest $TT_SERVER setup.sh c:/mozilla-build/python27/python.exe C:/mozilla-build/tooltool.py )
    fi
fi
