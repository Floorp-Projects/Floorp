from os import environ

# Loop server configuration
CONTENT_SERVER_PORT = 3001
LOOP_SERVER_PORT = environ.get("LOOP_SERVER_PORT") or 5001
LOOP_SERVER_URL = "http://localhost:" + str(LOOP_SERVER_PORT)
FIREFOX_PREFERENCES = {
    "loop.server": LOOP_SERVER_URL + "/v0",
    "browser.dom.window.dump.enabled": True,
    # Some more changes might be necesarry to have this working in offline mode
    "media.peerconnection.use_document_iceservers": False,
    "media.peerconnection.ice.loopback": True,
    "devtools.chrome.enabled": True,
    "devtools.debugger.prompt-connection": False,
    "devtools.debugger.remote-enabled": True,
    "media.volume_scale": "0",
    "loop.gettingStarted.seen": True,

    # this dialog is fragile, and likely to introduce intermittent failures
    "media.navigator.permission.disabled": True,
    # Use fake streams only
    "media.navigator.streams.fake": True
}
