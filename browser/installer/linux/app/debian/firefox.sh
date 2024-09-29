#!/bin/sh

FIREFOX="$(command -v firefox)"
[ -x "$FIREFOX.real" ] && exec "$FIREFOX.real" "$@"

exec firefox-esr "$@"
