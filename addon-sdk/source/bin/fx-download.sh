#!/bin/sh

if [ "$JPM_FX_DEBUG" = "1" ]; then
  fx-download --branch nightly -c prerelease --host ftp.mozilla.org ../firefox --debug
else
  fx-download --branch nightly -c prerelease --host ftp.mozilla.org ../firefox
fi
