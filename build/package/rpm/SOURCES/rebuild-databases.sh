#!/bin/sh

umask 022

if [ -f /usr/lib/mozilla/regxpcom ]; then

    /bin/rm -rf /usr/lib/mozilla/chrome/overlayinfo
    /bin/rm -f /usr/lib/mozilla/chrome/*.rdf
    /bin/mkdir -p /usr/lib/mozilla/chrome/overlayinfo
    /bin/rm -f /usr/lib/mozilla/component.reg

    MOZILLA_FIVE_HOME=/usr/lib/mozilla \
	/usr/lib/mozilla/regxpcom >/dev/null 2>/dev/null

    MOZILLA_FIVE_HOME=/usr/lib/mozilla \
	/usr/lib/mozilla/regchrome >/dev/null 2>/dev/null
fi

exit 0
