# Loop server configuration
CONTENT_SERVER_PORT = 3001
LOOP_SERVER_PORT = 5001
FIREFOX_PREFERENCES = {
    "loop.server": "http://localhost:" + str(LOOP_SERVER_PORT),
    "browser.dom.window.dump.enabled": True,
    # Need to find the correct Pythonic syntax for this (unless just using
    # a string is the way to go), as well as find the bug with the
    # other preference for shunting ice to localhost that ekr/drno did,
    # and other stuff before we can support offline call tests
    # XXX "media.peerconnection.default_iceservers": "[]",
    "media.peerconnection.use_document_iceservers": False,
    "devtools.chrome.enabled": True,
    "devtools.debugger.prompt-connection": False,
    "devtools.debugger.remote-enabled": True,
    "media.volume_scale": "0",

    # this dialog is fragile, and likely to introduce intermittent failures
    "media.navigator.permission.disabled": True
}
