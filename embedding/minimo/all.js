pref("general.useragent.override", "Mozilla/5.0 (X11; U; Familiar 0.72; en-US; rv:1.7a) Gecko/20040218 Minimo/0.1");

pref("keyword.enabled", false);

pref("browser.cache.memory.enable",         true);
pref("browser.cache.memory.capacity",       256);
// -1 = determine dynamically, 0 = none, n = memory capacity in kilobytes

// 0 = once-per-session, 1 = each-time, 2 = never, 3 = when-appropriate/automatically
pref("browser.cache.check_doc_frequency",   3);

pref("browser.display.use_document_fonts",  1);  // 0 = never, 1 = quick, 2 = always
pref("browser.display.use_document_colors", true);
pref("browser.display.use_system_colors",   false);
pref("browser.display.foreground_color",    "#000000");
pref("browser.display.background_color",    "#FFFFFF");
pref("browser.display.force_inline_alttext", false); // true = force ALT text for missing images to be layed out inline

// 0 = no external leading, 
// 1 = use external leading only when font provides, 
// 2 = add extra leading both internal leading and external leading are zero
pref("browser.display.normal_lineheight_calc_control", 2);
pref("browser.display.show_image_placeholders", true); // true = show image placeholders while image is loaded and when image is broken
pref("browser.anchor_color",                "#0000EE");
pref("browser.active_color",                "#EE0000");
pref("browser.visited_color",               "#551A8B");
pref("browser.underline_anchors",           true);
pref("browser.blink_allowed",               true);
pref("browser.enable_automatic_image_resizing", false);

pref("browser.display.use_focus_colors",    false);
pref("browser.display.focus_background_color", "#117722");
pref("browser.display.focus_text_color",     "#ffffff");
pref("browser.display.focus_ring_width",     1);
pref("browser.display.focus_ring_on_anything", false);

// Dialog modality issues
pref("browser.prefWindowModal", true);
pref("browser.show_about_as_stupid_modal_window", false);

pref("browser.sessionhistory.max_entries", 2);

// loading and rendering of framesets and iframes
pref("browser.frames.enabled", true);

// form submission
pref("browser.forms.submit.backwards_compatible", true);

// dispatch left clicks only to content in browser (still allows clicks to chrome/xul)
pref("nglayout.events.dispatchLeftClickOnly", true);

// whether or not to use xbl form controls
pref("nglayout.debug.enable_xbl_forms", false);

// size of scrollbar snapping region
pref("slider.snapMultiplier", 6);

// URI fixup prefs
pref("browser.fixup.alternate.enabled", true);
pref("browser.fixup.alternate.prefix", "www.");
pref("browser.fixup.alternate.suffix", ".com");
pref("browser.fixup.hide_user_pass", true);

// Scripts & Windows prefs
pref("browser.block.target_new_window",     false);
pref("dom.disable_image_src_set",           false);
pref("dom.disable_window_flip",             false);
pref("dom.disable_window_move_resize",      false);
pref("dom.disable_window_status_change",    false);

pref("dom.disable_window_open_feature.titlebar",    false);
pref("dom.disable_window_open_feature.close",       false);
pref("dom.disable_window_open_feature.toolbar",     false);
pref("dom.disable_window_open_feature.location",    false);
pref("dom.disable_window_open_feature.directories", false);
pref("dom.disable_window_open_feature.personalbar", false);
pref("dom.disable_window_open_feature.menubar",     false);
pref("dom.disable_window_open_feature.scrollbars",  false);
pref("dom.disable_window_open_feature.resizable",   false);
pref("dom.disable_window_open_feature.minimizable", false);
pref("dom.disable_window_open_feature.status",      false);

pref("dom.allow_scripts_to_close_windows",          false);

pref("javascript.enabled",                  true);
pref("javascript.allow.mailnews",           false);
pref("javascript.options.strict",           false);
pref("javascript.options.showInConsole",    true);

// advanced prefs
pref("advanced.always_load_images",         true);
pref("security.enable_java",                true);
pref("image.animation_mode",                "normal");

// If there is ever a security firedrill that requires
// us to block certian ports global, this is the pref 
// to use.  Is is a comma delimited list of port numbers
// for example:
//   pref("network.security.ports.banned", "1,2,3,4,5");
// prevents necko connecting to ports 1-5 unless the protocol
// overrides.

// Default action for unlisted external protocol handlers
pref("network.protocol-handler.external-default", true);      // OK to load
pref("network.protocol-handler.warn-external-default", true); // warn before load

// Prevent using external protocol handlers for these schemes
pref("network.protocol-handler.external.hcp", false);
pref("network.protocol-handler.external.vbscript", false);
pref("network.protocol-handler.external.javascript", false);
pref("network.protocol-handler.external.data", false);
pref("network.protocol-handler.external.ms-help", false);
pref("network.protocol-handler.external.vnd.ms.radio", false);
pref("network.protocol-handler.external.help", false);
pref("network.protocol-handler.external.disk", false);

// An exposed protocol handler is one that can be used in all contexts.  A
// non-exposed protocol handler is one that can only be used internally by the
// application.  For example, a non-exposed protocol would not be loaded by the
// application in response to a link click or a X-remote openURL command.
// Instead, it would be deferred to the system's external protocol handler.
// Only internal/built-in protocol handlers can be marked as exposed.

// This pref controls the default settings.  Per protocol settings can be used
// to override this value.
pref("network.protocol-handler.expose-all", true);

pref("intl.accept_languages",               "chrome://navigator/locale/navigator.properties");

// <http>
pref("network.http.version", "1.1");	  // default
// pref("network.http.version", "1.0");   // uncomment this out in case of problems
// pref("network.http.version", "0.9");   // it'll work too if you're crazy
// keep-alive option is effectively obsolete. Nevertheless it'll work with
// some older 1.0 servers:

pref("network.http.proxy.version", "1.1");    // default
// pref("network.http.proxy.version", "1.0"); // uncomment this out in case of problems
                                              // (required if using junkbuster proxy)

// enable caching of http documents
pref("network.http.use-cache", false);

// this preference can be set to override the socket type used for normal
// HTTP traffic.  an empty value indicates the normal TCP/IP socket type.
pref("network.http.default-socket-type", "");

pref("network.http.keep-alive", true); // set it to false in case of problems
pref("network.http.proxy.keep-alive", true);
pref("network.http.keep-alive.timeout", 300);

// limit the absolute number of http connections.
pref("network.http.max-connections", 6);

// limit the absolute number of http connections that can be established per
// host.  if a http proxy server is enabled, then the "server" is the proxy
// server.  Otherwise, "server" is the http origin server.
pref("network.http.max-connections-per-server", 2);

// if network.http.keep-alive is true, and if NOT connecting via a proxy, then
// a new connection will only be attempted if the number of active persistent
// connections to the server is less then max-persistent-connections-per-server.
pref("network.http.max-persistent-connections-per-server", 2);

// if network.http.keep-alive is true, and if connecting via a proxy, then a
// new connection will only be attempted if the number of active persistent
// connections to the proxy is less then max-persistent-connections-per-proxy.
pref("network.http.max-persistent-connections-per-proxy", 2);

// amount of time (in seconds) to suspend pending requests, before spawning a
// new connection, once the limit on the number of persistent connections per
// host has been reached.  however, a new connection will not be created if
// max-connections or max-connections-per-server has also been reached.
pref("network.http.request.max-start-delay", 10);

// http specific network timeouts (XXX currently unused)
pref("network.http.connect.timeout",  30);	// in seconds
pref("network.http.request.timeout", 120);	// in seconds

// Headers
pref("network.http.accept.default", "text/xml,application/xml,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,image/png,image/jpeg,image/gif;q=0.2,*/*;q=0.1");

pref("network.http.sendRefererHeader",      2); // 0=don't send any, 1=send only on clicks, 2=send on image requests as well

// Controls whether we send HTTPS referres to other HTTPS sites.
// By default this is enabled for compatibility (see bug 141641)
pref("network.http.sendSecureXSiteReferrer", true);

// Maximum number of consecutive redirects before aborting.
pref("network.http.redirection-limit", 20);

// Enable http compression: comment this out in case of problems with 1.1
// NOTE: support for "compress" has been disabled per bug 196406.
pref("network.http.accept-encoding" ,"gzip,deflate");

pref("network.http.pipelining"      , false);
pref("network.http.proxy.pipelining", false);

// Always pipeling the very first request:  this will only work when you are
// absolutely sure the the site or proxy you are browsing to/through support
// pipelining; the default behavior will be that the browser will first make
// a normal, non-pipelined request, then  examine  and remember the responce
// and only the subsequent requests to that site will be pipeline
pref("network.http.pipelining.firstrequest", false);

// Max number of requests in the pipeline
pref("network.http.pipelining.maxrequests" , 4);

// </http>

// This preference controls whether or not internationalized domain names (IDN)
// are handled.  IDN requires a nsIIDNService implementation.
pref("network.enableIDN", true);

// This preference controls whether or not URLs with UTF-8 characters are
// escaped.  Set this preference to TRUE for strict RFC2396 conformance.
pref("network.standard-url.escape-utf8", true);

// enables the prefetch service (i.e., prefetching of <link rel="next"> URLs).
pref("network.prefetch-next", true);

pref("network.image.imageBehavior",         0); // 0-Accept, 1-dontAcceptForeign, 2-dontUse
pref("network.image.warnAboutImages",       false);
pref("network.proxy.autoconfig_url",        "");
pref("network.proxy.type",                  0);
pref("network.proxy.ftp",                   "");
pref("network.proxy.ftp_port",              0);
pref("network.proxy.gopher",                "");
pref("network.proxy.gopher_port",           0);
pref("network.proxy.http",                  "");
pref("network.proxy.http_port",             0);
pref("network.proxy.ssl",                   "");
pref("network.proxy.ssl_port",              0);
pref("network.proxy.socks",                 "");
pref("network.proxy.socks_port",            0);
pref("network.proxy.socks_version",         5);
pref("network.proxy.no_proxies_on",         "localhost, 127.0.0.1");
pref("network.online",                      true); //online/offline
pref("network.cookie.cookieBehavior",       0); // 0-Accept, 1-dontAcceptForeign, 2-dontUse, 3-p3p
pref("network.cookie.disableCookieForMailNews", true); // disable all cookies for mail
pref("network.cookie.warnAboutCookies",     false);
pref("network.cookie.lifetime.enabled",     false);
pref("network.cookie.lifetime.behavior",    0);
pref("network.cookie.lifetime.days",        90);

pref("intl.accept_languages",               "chrome://navigator/locale/navigator.properties");
pref("intl.accept_charsets",                "iso-8859-1,*,utf-8");
pref("intl.collationOption",                "chrome://navigator-platform/locale/navigator.properties");
pref("intl.menuitems.alwaysappendaccesskeys","chrome://navigator/locale/navigator.properties");
pref("intl.charset.detector",               "chrome://navigator/locale/navigator.properties");
pref("intl.charset.default",                "chrome://navigator-platform/locale/navigator.properties");
pref("intl.content.langcode",               "chrome://communicator-region/locale/region.properties");
pref("intl.locale.matchOS",                 false);
// fallback charset list for Unicode conversion (converting from Unicode)
// currently used for mail send only to handle symbol characters (e.g Euro, trademark, smartquotes)
// for ISO-8859-1
pref("intl.fallbackCharsetList.ISO-8859-1", "windows-1252");
pref("font.language.group",                 "chrome://navigator/locale/navigator.properties");

pref("images.dither", "auto");
pref("security.directory",              "");

pref("signed.applets.codebase_principal_support", false);
pref("security.checkloaduri", true);
pref("security.xpconnect.plugin.unrestricted", true);

// Modifier key prefs: default to Windows settings,
// menu access key = alt, accelerator key = control.
// Use 17 for Ctrl, 18 for Alt, 224 for Meta, 0 for none. Mac settings in macprefs.js
pref("ui.key.accelKey", 17);
pref("ui.key.generalAccessKey", 18);
pref("ui.key.menuAccessKey", 18);
pref("ui.key.menuAccessKeyFocuses", false);
pref("ui.key.saveLink.shift", true); // true = shift, false = meta

// Clipboard behavior
pref("clipboard.autocopy", false);

pref("prefs.converted-to-utf8",false);

pref("browser.throbber.url","chrome://navigator-region/locale/region.properties");

// used for double-click word selection behavior. Win will override.
pref("layout.word_select.eat_space_to_next_word", false);
pref("layout.word_select.stop_at_punctuation", true);

// pref to force frames to be resizable
pref("layout.frames.force_resizability", false);

// if true, allow plug-ins to override internal imglib decoder mime types in full-page mode
pref("plugin.override_internal_types", false);
pref("plugin.expose_full_path", false); // if true navigator.plugins reveals full path

// Pref to control whether the viewmanager code does double-buffering or not
// See http://bugzilla.mozilla.org/show_bug.cgi?id=169483 for further details...
pref("viewmanager.do_doublebuffering", false);

pref("dom.disable_open_during_load",              false);

// whether use prefs from system
pref("config.use_system_prefs", false);

// if the system has enabled accessibility
pref("config.use_system_prefs.accessibility", false);

// Most Unix people think modal pref windows are stupid:
pref("browser.prefWindowModal", false);

// turn off scrollbar snapping
pref("slider.snapMultiplier", 0);

// default font sizes
pref("font.size.unit", "pt");
pref("font.size.fixed.ar", 9);
pref("font.size.fixed.x-baltic", 9);
pref("font.size.fixed.x-central-euro", 9);
pref("font.size.fixed.x-cyrillic", 9);
pref("font.size.fixed.x-devanagari", 9);
pref("font.size.fixed.el", 9);
pref("font.size.fixed.he", 9);
pref("font.size.fixed.ja", 9);
pref("font.size.fixed.ko", 9);
pref("font.size.fixed.zh-CN", 9);
pref("font.size.fixed.x-tamil", 9);
pref("font.size.fixed.th", 9);
pref("font.size.fixed.zh-TW", 9);
pref("font.size.fixed.zh-HK", 9);
pref("font.size.fixed.tr", 9);
pref("font.size.fixed.x-unicode", 9);
pref("font.size.fixed.x-western", 9);
pref("font.size.variable.ar", 8);
pref("font.size.variable.x-baltic", 8);
pref("font.size.variable.x-central-euro", 8);
pref("font.size.variable.x-cyrillic", 8);
pref("font.size.variable.x-devanagari", 8);
pref("font.size.variable.el", 8);
pref("font.size.variable.he", 8);
pref("font.size.variable.ja", 8);
pref("font.size.variable.ko", 8);
pref("font.size.variable.zh-CN", 8);
pref("font.size.variable.x-tamil", 8);
pref("font.size.variable.th", 8);
pref("font.size.variable.zh-TW", 8);
pref("font.size.variable.zh-HK", 8);
pref("font.size.variable.tr", 8);
pref("font.size.variable.x-unicode", 8);
pref("font.size.variable.x-western", 8);

