from os import environ

# Loop server configuration
CONTENT_SERVER_PORT = 3001
LOOP_SERVER_PORT = environ.get("LOOP_SERVER_PORT") or 5001
TEST_SERVER = environ.get("TEST_SERVER") or "dev"
USE_LOCAL_STANDALONE = environ.get("USE_LOCAL_STANDALONE") or "1"

CONTENT_SERVER_URL = environ.get("CONTENT_SERVER_URL") or \
    "http://localhost:" + str(CONTENT_SERVER_PORT)

LOOP_SERVER_URLS = {
    "local": "http://localhost:" + str(LOOP_SERVER_PORT),
    "dev": "https://loop-dev.stage.mozaws.net",
    "stage": "https://loop.stage.mozaws.net",
    "prod": "https://loop.services.mozilla.com"
}

LOOP_SERVER_URL = LOOP_SERVER_URLS[TEST_SERVER]

FIREFOX_PREFERENCES = {
    "loop.server": LOOP_SERVER_URL + "/v0",
    # XXX Bug 1254520. We fake this to something invalid so that we can run
    # functional tests against production without the "this is your own room
    # display" popping up. Ideally we should test that the own room display
    # works properly.
    "loop.linkClicker.url": "https://invalid/",
    "browser.dom.window.dump.enabled": True,
    # Some more changes might be necesarry to have this working in offline mode
    "media.peerconnection.use_document_iceservers": False,
    "media.peerconnection.ice.loopback": True,
    "devtools.chrome.enabled": True,
    "devtools.debugger.prompt-connection": False,
    "devtools.debugger.remote-enabled": True,
    "media.volume_scale": "0",
    "loop.gettingStarted.latestFTUVersion": 2,
    "loop.remote.autostart": True,

    # this dialog is fragile, and likely to introduce intermittent failures
    "media.navigator.permission.disabled": True,
    # Use fake streams only
    "media.navigator.streams.fake": True,

    # attempts to work around Ubuntu wanting to install add-ons
    "extensions.enabledScopes": 5,
    "extensions.autoDisableScopes": 0,
    "extensions.update.enabled": False,
    "extensions.installDistroAddons": False,
    "extensions.blocklist.enabled": False,
    "extensions.update.notifyUser": False,
    "xpinstall.signatures.required": False,
}
