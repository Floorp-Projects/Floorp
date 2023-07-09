if [[ $GHA_aarch64 != 'true' ]]; then
  rm -rf ./l10n-central/.git
fi

Xvfb :2 -screen 0 1024x768x24 &
export DISPLAY=:2

export WORKDIR=`pwd`
echo "Current Workdir: $WORKDIR"
./mach build