# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

DEFAULT_COMMON_PREFS = {
    # allow debug output via dump to be printed to the system console
    # (setting it here just in case, even though PlainTextConsole also
    # sets this preference)
    'browser.dom.window.dump.enabled': True,
    # warn about possibly incorrect code
    'javascript.options.showInConsole': True,

    # Allow remote connections to the debugger
    'devtools.debugger.remote-enabled' : True,

    'extensions.sdk.console.logLevel': 'info',

    'extensions.checkCompatibility.nightly' : False,

    # Disable extension updates and notifications.
    'extensions.update.enabled' : False,
    'lightweightThemes.update.enabled' : False,
    'extensions.update.notifyUser' : False,

    # From:
    # http://hg.mozilla.org/mozilla-central/file/1dd81c324ac7/build/automation.py.in#l372
    # Only load extensions from the application and user profile.
    # AddonManager.SCOPE_PROFILE + AddonManager.SCOPE_APPLICATION
    'extensions.enabledScopes' : 5,
    # Disable metadata caching for installed add-ons by default
    'extensions.getAddons.cache.enabled' : False,
    # Disable intalling any distribution add-ons
    'extensions.installDistroAddons' : False,
    # Allow installing extensions dropped into the profile folder
    'extensions.autoDisableScopes' : 10,

    # shut up some warnings on `about:` page
    'app.releaseNotesURL': 'http://localhost/app-dummy/',
    'app.vendorURL': 'http://localhost/app-dummy/',
}

DEFAULT_NO_CONNECTIONS_PREFS = {
    'toolkit.telemetry.enabled': False,
    'toolkit.telemetry.server': 'https://localhost/telemetry-dummy/',
    'app.update.auto' : False,
    'app.update.url': 'http://localhost/app-dummy/update',
    # Make sure GMPInstallManager won't hit the network.
    'media.gmp-gmpopenh264.autoupdate' : False,
    'media.gmp-manager.cert.checkAttributes' : False,
    'media.gmp-manager.cert.requireBuiltIn' : False,
    'media.gmp-manager.url' : 'http://localhost/media-dummy/gmpmanager',
    'media.gmp-manager.url.override': 'http://localhost/dummy-gmp-manager.xml',
    'media.gmp-manager.updateEnabled': False,
    'browser.aboutHomeSnippets.updateUrl': 'https://localhost/snippet-dummy',
    'browser.newtab.url' : 'about:blank',
    'browser.search.update': False,
    'browser.search.suggest.enabled' : False,
    'browser.safebrowsing.phishing.enabled' : False,
    'browser.safebrowsing.provider.google.updateURL': 'http://localhost/safebrowsing-dummy/update',
    'browser.safebrowsing.provider.google.gethashURL': 'http://localhost/safebrowsing-dummy/gethash',
    'browser.safebrowsing.malware.reportURL': 'http://localhost/safebrowsing-dummy/malwarereport',
    'browser.selfsupport.url': 'https://localhost/selfsupport-dummy',
    'browser.safebrowsing.provider.mozilla.gethashURL': 'http://localhost/safebrowsing-dummy/gethash',
    'browser.safebrowsing.provider.mozilla.updateURL': 'http://localhost/safebrowsing-dummy/update',

    # Disable app update
    'app.update.enabled' : False,
    'app.update.staging.enabled': False,

    # Disable about:newtab content fetch and ping
    'browser.newtabpage.directory.source': 'data:application/json,{"jetpack":1}',
    'browser.newtabpage.directory.ping': '',

    # Point update checks to a nonexistent local URL for fast failures.
    'extensions.update.url' : 'http://localhost/extensions-dummy/updateURL',
    'extensions.update.background.url': 'http://localhost/extensions-dummy/updateBackgroundURL',
    'extensions.blocklist.url' : 'http://localhost/extensions-dummy/blocklistURL',
    # Make sure opening about:addons won't hit the network.
    'extensions.webservice.discoverURL' : 'http://localhost/extensions-dummy/discoveryURL',
    'extensions.getAddons.maxResults': 0,

    # Disable webapp updates.  Yes, it is supposed to be an integer.
    'browser.webapps.checkForUpdates': 0,

    # Location services
    'geo.wifi.uri': 'http://localhost/location-dummy/locationURL',
    'browser.search.geoip.url': 'http://localhost/location-dummy/locationURL',

    # Tell the search service we are running in the US.  This also has the
    # desired side-effect of preventing our geoip lookup.
    'browser.search.isUS' : True,
    'browser.search.countryCode' : 'US',

    'geo.wifi.uri' : 'http://localhost/extensions-dummy/geowifiURL',
    'geo.wifi.scan' : False,

    # We don't want to hit the real Firefox Accounts server for tests.  We don't
    # actually need a functioning FxA server, so just set it to something that
    # resolves and accepts requests, even if they all fail.
    'identity.fxaccounts.auth.uri': 'http://localhost/fxa-dummy/'
}

DEFAULT_FENNEC_PREFS = {
  'browser.console.showInPanel': True,
  'browser.firstrun.show.uidiscovery': False
}

# When launching a temporary new Firefox profile, use these preferences.
DEFAULT_FIREFOX_PREFS = {
    'browser.startup.homepage' : 'about:blank',
    'startup.homepage_welcome_url' : 'about:blank',
    'devtools.browsertoolbox.panel': 'jsdebugger',
    'devtools.chrome.enabled' : True,

    # From:
    # http://hg.mozilla.org/mozilla-central/file/1dd81c324ac7/build/automation.py.in#l388
    # Make url-classifier updates so rare that they won't affect tests.
    'urlclassifier.updateinterval' : 172800,
    # Point the url-classifier to a nonexistent local URL for fast failures.
    'browser.safebrowsing.provider.google.gethashURL' : 'http://localhost/safebrowsing-dummy/gethash',
    'browser.safebrowsing.provider.google.updateURL' : 'http://localhost/safebrowsing-dummy/update',
    'browser.safebrowsing.provider.mozilla.gethashURL': 'http://localhost/safebrowsing-dummy/gethash',
    'browser.safebrowsing.provider.mozilla.updateURL': 'http://localhost/safebrowsing-dummy/update',
}

# When launching a temporary new Thunderbird profile, use these preferences.
# Note that these were taken from:
# http://dxr.mozilla.org/comm-central/source/mail/test/mozmill/runtest.py
DEFAULT_THUNDERBIRD_PREFS = {
    # say no to slow script warnings
    'dom.max_chrome_script_run_time': 200,
    'dom.max_script_run_time': 0,
    # do not ask about being the default mail client
    'mail.shell.checkDefaultClient': False,
    # disable non-gloda indexing daemons
    'mail.winsearch.enable': False,
    'mail.winsearch.firstRunDone': True,
    'mail.spotlight.enable': False,
    'mail.spotlight.firstRunDone': True,
    # disable address books for undisclosed reasons
    'ldap_2.servers.osx.position': 0,
    'ldap_2.servers.oe.position': 0,
    # disable the first use junk dialog
    'mailnews.ui.junk.firstuse': False,
    # other unknown voodoo
    # -- dummied up local accounts to stop the account wizard
    'mail.account.account1.server' :  "server1",
    'mail.account.account2.identities' :  "id1",
    'mail.account.account2.server' :  "server2",
    'mail.accountmanager.accounts' :  "account1,account2",
    'mail.accountmanager.defaultaccount' :  "account2",
    'mail.accountmanager.localfoldersserver' :  "server1",
    'mail.identity.id1.fullName' :  "Tinderbox",
    'mail.identity.id1.smtpServer' :  "smtp1",
    'mail.identity.id1.useremail' :  "tinderbox@invalid.com",
    'mail.identity.id1.valid' :  True,
    'mail.root.none-rel' :  "[ProfD]Mail",
    'mail.root.pop3-rel' :  "[ProfD]Mail",
    'mail.server.server1.directory-rel' :  "[ProfD]Mail/Local Folders",
    'mail.server.server1.hostname' :  "Local Folders",
    'mail.server.server1.name' :  "Local Folders",
    'mail.server.server1.type' :  "none",
    'mail.server.server1.userName' :  "nobody",
    'mail.server.server2.check_new_mail' :  False,
    'mail.server.server2.directory-rel' :  "[ProfD]Mail/tinderbox",
    'mail.server.server2.download_on_biff' :  True,
    'mail.server.server2.hostname' :  "tinderbox",
    'mail.server.server2.login_at_startup' :  False,
    'mail.server.server2.name' :  "tinderbox@invalid.com",
    'mail.server.server2.type' :  "pop3",
    'mail.server.server2.userName' :  "tinderbox",
    'mail.smtp.defaultserver' :  "smtp1",
    'mail.smtpserver.smtp1.hostname' :  "tinderbox",
    'mail.smtpserver.smtp1.username' :  "tinderbox",
    'mail.smtpservers' :  "smtp1",
    'mail.startup.enabledMailCheckOnce' :  True,
    'mailnews.start_page_override.mstone' :  "ignore",
}

DEFAULT_TEST_PREFS = {
    'browser.console.showInPanel': True,
    'browser.startup.page': 0,
    'browser.firstrun.show.localepicker': False,
    'browser.firstrun.show.uidiscovery': False,
    'browser.ui.layout.tablet': 0,
    'dom.disable_open_during_load': False,
    'dom.experimental_forms': True,
    'dom.forms.number': True,
    'dom.forms.color': True,
    'dom.max_script_run_time': 0,
    'hangmonitor.timeout': 0,
    'dom.max_chrome_script_run_time': 0,
    'dom.popup_maximum': -1,
    'dom.send_after_paint_to_content': True,
    'dom.successive_dialog_time_limit': 0,
    'browser.shell.checkDefaultBrowser': False,
    'shell.checkDefaultClient': False,
    'browser.warnOnQuit': False,
    'accessibility.typeaheadfind.autostart': False,
    'browser.EULA.override': True,
    'gfx.color_management.force_srgb': True,
    'network.manage-offline-status': False,
    # Disable speculative connections so they aren't reported as leaking when they're hanging around.
    'network.http.speculative-parallel-limit': 0,
    'test.mousescroll': True,
    # Need to client auth test be w/o any dialogs
    'security.default_personal_cert': 'Select Automatically',
    'network.http.prompt-temp-redirect': False,
    'security.warn_viewing_mixed': False,
    'extensions.defaultProviders.enabled': True,
    'datareporting.policy.dataSubmissionPolicyBypassNotification': True,
    'layout.css.report_errors': True,
    'layout.css.grid.enabled': True,
    'layout.spammy_warnings.enabled': False,
    'dom.mozSettings.enabled': True,
    # Make sure the disk cache doesn't get auto disabled
    'network.http.bypass-cachelock-threshold': 200000,
    # Always use network provider for geolocation tests
    # so we bypass the OSX dialog raised by the corelocation provider
    'geo.provider.testing': True,
    # Background thumbnails in particular cause grief, and disabling thumbnails
    # in general can't hurt - we re-enable them when tests need them.
    'browser.pagethumbnails.capturing_disabled': True,
    # Indicate that the download panel has been shown once so that whichever
    # download test runs first doesn't show the popup inconsistently.
    'browser.download.panel.shown': True,
    # Assume the about:newtab page's intro panels have been shown to not depend on
    # which test runs first and happens to open about:newtab
    'browser.newtabpage.introShown': True,
    # Disable useragent updates.
    'general.useragent.updates.enabled': False,
    'dom.mozApps.debug': True,
    'dom.apps.customization.enabled': True,
    'media.eme.enabled': True,
    'media.eme.apiVisible': True,
    # Don't forceably kill content processes after a timeout
    'dom.ipc.tabs.shutdownTimeoutSecs': 0,
    'general.useragent.locale': "en-US",
    'intl.locale.matchOS': "en-US",
    'dom.indexedDB.experimental': True
}
