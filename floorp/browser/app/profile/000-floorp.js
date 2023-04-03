#filter dumbComments emptyLines substitution

// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef XP_UNIX
  #ifndef XP_MACOSX
    #define UNIX_BUT_NOT_MAC
  #endif
#endif

/*----------------------------------------------------- Floorp 専用の pref 設定 ----------------------------------------------------*/
//Floorpアップデートの最新版である旨の通知を許可
pref("enable.floorp.updater.latest", false);
pref("enable.floorp.update", true);
#ifdef XP_MACOSX
pref("update.id.floorp", "stable");
#endif

#ifdef XP_WIN
pref("update.id.floorp", "windows");
#endif

#ifdef XP_LINUX
pref("update.id.floorp", "linux");
#endif

// userAgent
pref("floorp.browser.UserAgent", 0);
pref("floorp.general.useragent.override", "");

pref("floorp.chrome.theme.mode", -1);

//ブラウザーUIのカスタマイズ設定
pref("floorp.hide.tabbrowser-tab.enable", false); //水平タブ削除
pref("floorp.optimized.verticaltab", false); //ツリー型垂直タブ等に最適化。8.7.2 からフォーカスした際の動作は別に
pref("floorp.horizontal.tab.position.shift", false); //水平タブ位置移動
pref("floorp.Tree-type.verticaltab.optimization", false); //ツリー型タブのフォーカスした際の挙動を変更
pref("floorp.bookmarks.bar.focus.mode", false);//フォーカスした際にブックマークバーを展開
pref("floorp.material.effect.enable", false);//マテリアルモードの実装
pref("floorp.disable.fullscreen.notification", false);//フルスクリーン通知を無効化
pref("floorp.navbar.bottom", false);
pref("floorp.tabs.showPinnedTabsTitle", false); //ピン留めされたタブのタイトルを表示
pref("floorp.delete.browser.border", false); //ブラウザーの枠線削除＆丸くする

pref("floorp.browser.user.interface", 3);// Floorp 10 系以降のインターフェーステーマ設定
pref("floorp.browser.tabbar.settings", 0);// タブの設定

pref("floorp.enable.auto.restart", false);

pref("floorp.browser.rest.mode", false);// 休止モード


pref("browser.disable.nt.image.gb", false);// 画像を表示しない

pref("floorp.enable.dualtheme", false); //デュアルテーマの有効・無効 
pref("floorp.dualtheme.theme", "[]"); //デュアルテーマのリスト

pref("floorp.download.notification", 4); //ダウンロード通知

pref("contentblocking.cryptomining_blocking_enabled", true);
pref("contentblocking.cryptomining_blocking_enabled",true);
pref("contentblocking.category", true);
pref("privacy.trackingprotection.enabled", true);
pref("network.cookie.cookieBehavior", 5); 
pref("privacy.partition.serviceWorkers", true);
pref("privacy.restrict3rdpartystorage.rollout.preferences.TCPToggleInStandard", true);
pref("privacy.restrict3rdpartystorage.rollout.enabledByDefault", true);

//新しいタブの検索を「ウェブを検索」に変更
pref("browser.newtabpage.activity-stream.improvesearch.handoffToAwesomebar", false);
//新しいタブの背景の設定
pref("browser.newtabpage.activity-stream.floorp.background.type", 1);
pref("browser.newtabpage.activity-stream.floorp.background.images.folder", "");
pref("browser.newtabpage.activity-stream.floorp.background.images.extensions", "png,jpg,jpeg,webp,gif,svg,tiff,tif,bmp,avif");

pref("floorp.multitab.bottommode", false);

pref("browser.display.statusbar", false);


pref("floorp.browser.sidebar.right", true);// サイドバーの右側を表示
pref("floorp.browser.sidebar.enable", true);// サイドバーを表示

// url:URL width:幅 userAgent:userAgent usercontext:コンテナタブ
pref("floorp.browser.sidebar2.data", '{"data":{},"index":[]}');

pref("floorp.browser.sidebar2.global.webpanel.width", 400);

pref("floorp.tabsleep.enabled", false);
pref("floorp.webcompat.enabled", true);
pref("floorp.openLinkInExternal.enabled", false);
pref("floorp.openLinkInExternal.browserId", "");

// システムアドオンのアップデート確認先
pref("extensions.systemAddon.update.url", "https://floorp-update.ablaze.one/systemAddon/xml/%DISPLAY_VERSION%/%OS%/update.xml");

// 言語設定をシステムに合わせる
pref("intl.locale.requested", "");

pref("app.feedback.baseURL", "https://support.ablaze.one/");

// 多段タブ
pref("floorp.enable.multitab", false);
pref("floorp.browser.tabbar.multirow.max.enabled", true);
pref("floorp.browser.tabbar.multirow.newtab-inside.enabled", false);
pref("floorp.browser.tabbar.multirow.max.row", 3);

// Floorp Notes
pref("floorp.browser.note.memos", "");
pref("floorp.browser.note.memos.using", -1);
pref("services.sync.prefs.sync.floorp.browser.note.memos", true);
pref("floorp.browser.note.enabled", false);

//Clock
pref("floorp.browser.clock.enabled", false);

//BMS Search
pref("browser.search.useDefault.enabled", false);

//新規タブの開く位置
pref("floorp.browser.tabs.openNewTabPosition", -1);

//ネイティブ実装垂直タブ
pref("floorp.browser.native.verticaltabs.enabled", false);
/*----------------------------------------------------------------------------------------------------------------------------------*/

//検索エンジン
pref("browser.search.separatePrivateDefault", true);
pref("browser.search.separatePrivateDefault.ui.enabled", true);
pref("browser.urlbar.update2.engineAliasRefresh", true);
pref("devtools.debugger.prompt-connection", false);

//個人設定の同期無効
pref("services.sync.engine.prefs", false); // Never sync prefs, addons, or tabs with other browsers
pref("services.sync.prefs.sync.browser.newtabpage.activity-stream.feeds.snippets", false, locked);
pref("services.sync.prefs.sync.browser.newtabpage.activity-stream.showSponsored", false, locked);
pref("services.sync.prefs.sync.browser.newtabpage.activity-stream.showSponsoredTopSites", false, locked);
pref("services.sync.telemetry.maxPayloadCount", "0", locked);
pref("services.sync.telemetry.submissionInterval", "0", locked);
pref("services.sync.prefs.sync.browser.startup.page", false, locked); // Firefox の自動復元機能を Firefox Sync で同期しないようにします。
pref("services.sync.prefs.sync.browser.tabs.warnOnClose", false, locked); //たくさんのタブを閉じようとした際の警告表示を Firefox Sync で同期しないようにします。

// 同期を有効にする
pref("services.sync.prefs.sync.floorp.browser.sidebar.right", true);// サイドバーの右側を表示
pref("services.sync.prefs.sync.floorp.browser.sidebar2.data", true);// サイドバーのデータ
pref("services.sync.prefs.sync.floorp.optimized.verticaltab", true); //ツリー型垂直タブ等に最適化。8.7.2 からフォーカスした際の動作は別に
pref("services.sync.prefs.sync.floorp.browser.user.interface", true);// Floorp 10 系以降のインターフェーステーマ設定
pref("services.sync.prefs.sync.floorp.browser.tabbar.settings", true);// タブの設定

pref("toolkit.legacyUserProfileCustomizations.stylesheets" ,true);

pref("browser.preferences.moreFromMozilla", false, locked);

//たくさん閉じようとしたときに警告
pref("browser.tabs.warnOnClose", true);
pref("browser.tabs.warnOnCloseOtherTabs", true);

//デフォルトブラウザーのチェック無効
pref("browser.shell.checkDefaultBrowser", false, locked);

//addon推奨プロンプトを消す
pref("extensions.getAddons.showPane", false);

//調査と思われるものを削除。Torでは削除済み。
pref("app.normandy.api_url", "");
pref("app.normandy.enabled", true);

//backdropfilterを既定で有効化します。
pref("layout.css.backdrop-filter.enabled", true);

//SVG avif 画像ファイルをの互換性向上または、既定で開けるように
pref("svg.context-properties.content.enabled", true, locked);
pref("image.avif.enabled", true, locked);

// Add-On のブラックリストをFloorpが参照する際の情報漏洩削減
pref("extensions.blocklist.url", "https://blocklist.addons.mozilla.org/blocklist/3/%APP_ID%/%APP_VERSION%/");

//ブラックリストの参照の有効化
pref("extensions.blocklist.enabled", true);
pref("services.blocklist.update_enabled",	true);

// Add-On のブラックリストをFloorpが参照する際の情報漏洩削減のために、先にリンク先を指定
pref("extensions.blocklist.url", "https://blocklist.addons.mozilla.org/blocklist/3/%APP_ID%/%APP_VERSION%/");

//Pocket機能を無効化*/
pref("extensions.pocket.enabled", false);

//コンテンツセキュリティポリシー(CSP)の有効化
pref("security.csp.enable", true);

//クラッシュレポートを無効化
pref("breakpad.reportURL", "", locked);
pref("browser.tabs.crashReporting.sendReport", false);
pref("browser.crashReports.unsubmittedCheck.enabled",	false);

//野良アドオンのインストールを許可。開発者向け。Floorp アドオンストアの準備。
pref("xpinstall.signatures.required", false);

// Firefox による、Mozilla への情報送信テレメンタリーを無効化
pref("toolkit.telemetry.archive.enabled", false, locked);
pref("toolkit.telemetry.bhrPing.enabled", false, locked);
pref("toolkit.telemetry.enabled", false, locked);
pref("toolkit.telemetry.firstShutdownPing.enabled", false, locked);
pref("toolkit.telemetry.geckoview.streaming", false, locked);
pref("toolkit.telemetry.newProfilePing.enabled", false, locked);
pref("toolkit.telemetry.pioneer-new-studies-available", false, locked);
pref("toolkit.telemetry.reportingpolicy.firstRun", false, locked);
pref("toolkit.telemetry.server", "", locked);
pref("toolkit.telemetry.shutdownPingSender.enabled", false, locked);
pref("toolkit.telemetry.shutdownPingSender.enabledFirstSession", false, locked);
pref("toolkit.telemetry.testing.overrideProductsCheck", false, locked);
pref("toolkit.telemetry.unified", false, locked);
pref("toolkit.telemetry.updatePing.enabled", false, locked);

//Firefox調査を無効化
pref("app.shield.optoutstudies.enabled", false, locked);

//拡張機能の推奨を削除
pref("browser.discovery.enabled", false);

//クラッシュレポートの自動送信無効
pref("browser.crashReports.unsubmittedCheck.autoSubmit2", false);

// CPUのコア数を偽装し、正しい値をウェブサイトに返さないようにします。
pref("dom.maxHardwareConcurrency",	2);

//Mozillaが提供する位置特定型検索エンジンを使用しない。位置情報がより保護されます。
pref("browser.search.geoSpecificDefaults", false);

//http 通信時、Floorp は絶対にhttp:// をURLバーから隠しません
pref("browser.urlbar.trimURLs", false);

//「既定でオンを推奨」フィンガープリント対策の一環。
//参考：https://www.torproject.org/projects/torbrowser/design/#fingerprinting-defenses
pref("dom.network.enabled", false);

// webRTCは完全に無効化しません。Brave・Safari では、既定で無効化されています。
pref("media.peerconnection.enabled", true);

//WebRTC が有効な場合、Floorp はできるだけ、IPアドレスを秘匿するよう動作します。
pref("media.peerconnection.ice.default_address_only", true);
pref("media.peerconnection.ice.no_host", true);

//アドオンのフィンガープリント採取から保護
pref("privacy.resistFingerprinting.block_mozAddonManager", true);

//プライバシー機能をオンにし、テレメトリー採取を無効化します。
pref("privacy.trackingprotection.origin_telemetry.enabled", false, locked);
pref("privacy.userContext.enabled", true);
pref("privacy.userContext.ui.enabled", true);
pref("trailhead.firstrun.branches", "", locked);
pref("extensions.webcompat-reporter.enabled", false);

pref("browser.startup.page", 3);//自動復元
pref("browser.tabs.closeWindowWithLastTab", false);//最後のタブを閉じてもブラウザが閉じないように]
pref("general.config.obscure_value", 0);

//font
pref("font.blacklist.underline_offset", "FangSong,Gulim,GulimChe,MingLiU,MingLiU-ExtB,MingLiU_HKSCS,MingLiU-HKSCS-ExtB,MS Gothic,MS Mincho,MS PGothic,MS PMincho,MS UI Gothic,PMingLiU,PMingLiU-ExtB,SimHei,SimSun,SimSun-ExtB,Hei,Kai,Apple LiGothic,Apple LiSung,Osaka,Meiryo");

// https://developer.mozilla.org/docs/Web/API/Navigator/share
#ifdef XP_WIN
pref("dom.webshare.enabled", true);
#endif

/*-----------------------------------------------------------------------------------all.js の設定-----------------------------------------------------------------------------------*/

pref("extensions.htmlaboutaddons.recommendations.enabled", false, locked);
pref("datareporting.policy.dataSubmissionEnable", false, locked);
pref("datareporting.healthreport.uploadEnabled", false, locked);
pref("general.config.sandbox_enabled", false);
pref("toolkit.legacyUserProfileCustomizations.script", false);

/*-----------------------------------------------------------------------------以下、Photon の既定の設定-----------------------------------------------------------------------------*/
//Floorp

// 1 = photon, 2 = lepton
pref("floorp.lepton.interface", 1);

// ** Theme Default Options ****************************************************
// userchrome.css usercontent.css activate
pref("toolkit.legacyUserProfileCustomizations.stylesheets", true);

// Proton Enabled #127 || Removed at 97 #328 (Maintained for compatibility with ESR)
pref("browser.proton.enabled", true);

// Proton Tooltip
pref("browser.proton.places-tooltip.enabled", true);

// Fill SVG Color
pref("svg.context-properties.content.enabled", true);

// CSS Color Mix - 88 Above
pref("layout.css.color-mix.enabled", true);

// CSS Blur Filter - 88 Above
pref("layout.css.backdrop-filter.enabled", true);

// Restore Compact Mode - 89 Above
pref("browser.compactmode.show", true);

// about:home Search Bar - 89 Above
pref("browser.newtabpage.activity-stream.improvesearch.handoffToAwesomebar", false);

// CSS's `:has()` selector #457 - 103 Above
pref("layout.css.has-selector.enabled", true);

// Browser Theme Based Scheme - Will be activate 95 Above
// pref("layout.css.prefers-color-scheme.content-override", 3);

// ** Theme Related Options ****************************************************
// == Theme Distribution Settings ==============================================
// The rows that are located continuously must be changed `true`/`false` explicitly because there is a collision.
// https://github.com/black7375/Firefox-UI-Fix/wiki/Options#important
pref("userChrome.tab.connect_to_window",          true); // Original, Photon
pref("userChrome.tab.color_like_toolbar",         true); // Original, Photon

pref("userChrome.tab.lepton_like_padding",       false); // Original
pref("userChrome.tab.photon_like_padding",        true); // Photon

pref("userChrome.tab.dynamic_separator",         false); // Original, Proton
pref("userChrome.tab.static_separator",           true); // Photon
pref("userChrome.tab.static_separator.selected_accent", false); // Just option

pref("userChrome.tab.newtab_button_like_tab",    false); // Original
pref("userChrome.tab.newtab_button_smaller",      true); // Photon
pref("userChrome.tab.newtab_button_proton",      false); // Proton

pref("userChrome.icon.panel_full",               false); // Original, Proton
pref("userChrome.icon.panel_photon",              true); // Photon
pref("userChrome.icon.panel_sparse",             false); // Just option

// Original Only
pref("userChrome.tab.box_shadow",                false);
pref("userChrome.tab.bottom_rounded_corner",     false);

// Photon Only
pref("userChrome.tab.photon_like_contextline",    true);
pref("userChrome.rounding.square_tab",            true);

// == Theme Compatibility Settings =============================================
// pref("userChrome.compatibility.accent_color",         true); // Firefox v103 Below
// pref("userChrome.compatibility.covered_header_image", true);
// pref("userChrome.compatibility.panel_cutoff",         true);
// pref("userChrome.compatibility.navbar_top_border",    true);
// pref("userChrome.compatibility.dynamic_separator",    true); // Need dynamic_separator

// pref("userChrome.compatibility.os.linux_non_native_titlebar_button", true);
// pref("userChrome.compatibility.os.windows_maximized", true);

// == Theme Custom Settings ====================================================
// -- User Chrome --------------------------------------------------------------
// pref("userChrome.theme.proton_color.dark_blue_accent", true);
// pref("userChrome.theme.monospace",                     true);

// pref("userChrome.decoration.disable_panel_animate",    true);
// pref("userChrome.decoration.disable_sidebar_animate",  true);

// pref("userChrome.autohide.tab",                        true);
// pref("userChrome.autohide.tab.opacity",                true);
// pref("userChrome.autohide.tab.blur",                   true);
// pref("userChrome.autohide.tabbar",                     true);
// pref("userChrome.autohide.navbar",                     true);
// pref("userChrome.autohide.bookmarkbar",                true);
// pref("userChrome.autohide.sidebar",                    true);
// pref("userChrome.autohide.fill_urlbar",                true);
// pref("userChrome.autohide.back_button",                true);
// pref("userChrome.autohide.forward_button",             true);
// pref("userChrome.autohide.page_action",                true);
// pref("userChrome.autohide.toolbar_overlap",            true);
// pref("userChrome.autohide.toolbar_overlap.allow_layout_shift", true);

// pref("userChrome.hidden.tab_icon",                     true);
// pref("userChrome.hidden.tab_icon.always",              true);
// pref("userChrome.hidden.tabbar",                       true);
// pref("userChrome.hidden.navbar",                       true);
// pref("userChrome.hidden.sidebar_header",               true);
// pref("userChrome.hidden.sidebar_header.vertical_tab_only", true);
// pref("userChrome.hidden.urlbar_iconbox",               true);
// pref("userChrome.hidden.bookmarkbar_icon",             true);
// pref("userChrome.hidden.bookmarkbar_label",            true);
// pref("userChrome.hidden.disabled_menu",                true);

// pref("userChrome.centered.tab",                        true);
// pref("userChrome.centered.tab.label",                  true);
// pref("userChrome.centered.urlbar",                     true);
// pref("userChrome.centered.bookmarkbar",                true);

// pref("userChrome.rounding.square_button",              true);
// pref("userChrome.rounding.square_panel",               true);
// pref("userChrome.rounding.square_panelitem",           true);
// pref("userChrome.rounding.square_menupopup",           true);
// pref("userChrome.rounding.square_menuitem",            true);
// pref("userChrome.rounding.square_field",               true);
// pref("userChrome.rounding.square_checklabel",          true);

// pref("userChrome.padding.first_tab",                   true);
// pref("userChrome.padding.first_tab.always",            true);
// pref("userChrome.padding.drag_space",                  true);
// pref("userChrome.padding.drag_space.maximized",        true);

// pref("userChrome.padding.menu_compact",                true);
// pref("userChrome.padding.bookmark_menu.compact",       true);
// pref("userChrome.padding.urlView_expanding",           true);
// pref("userChrome.padding.urlView_result",              true);
// pref("userChrome.padding.panel_header",                true);

// pref("userChrome.urlView.move_icon_to_left",           true);
// pref("userChrome.urlView.go_button_when_typing",       true);
// pref("userChrome.urlView.always_show_page_actions",    true);

// pref("userChrome.tabbar.as_titlebar",                  true);
// pref("userChrome.tabbar.on_bottom",                    true);
// pref("userChrome.tabbar.on_bottom.above_bookmark",     true); // Need on_bottom
// pref("userChrome.tabbar.on_bottom.menubar_on_top",     true); // Need on_bottom
// pref("userChrome.tabbar.on_bottom.hidden_single_tab",  true); // Need on_bottom
// pref("userChrome.tabbar.one_liner",                    true);
// pref("userChrome.tabbar.one_liner.combine_navbar",     true); // Need one_liner
// pref("userChrome.tabbar.one_liner.tabbar_first",       true); // Need one_liner
// pref("userChrome.tabbar.one_liner.responsive",         true); // Need one_liner

// pref("userChrome.tab.always_show_tab_icon",            true);
// pref("userChrome.tab.close_button_at_pinned",          true);
// pref("userChrome.tab.close_button_at_pinned.always",   true);
// pref("userChrome.tab.close_button_at_pinned.background", true);
// pref("userChrome.tab.close_button_at_hover.always",    true); // Need close_button_at_hover
// pref("userChrome.tab.sound_show_label",                true); // Need remove sound_hide_label

// pref("userChrome.panel.remove_strip",                  true);
// pref("userChrome.panel.full_width_separator",          true);
// pref("userChrome.panel.full_width_padding",            true);

// pref("userChrome.sidebar.overlap",                     true);

// pref("userChrome.icon.disabled",                       true);
// pref("userChrome.icon.account_image_to_right",         true);
// pref("userChrome.icon.account_label_to_right",         true);
// pref("userChrome.icon.menu.full",                      true);
// pref("userChrome.icon.global_menu.mac",                true);

// -- User Content -------------------------------------------------------------
// pref("userContent.player.ui.twoline",                  true);

// pref("userContent.page.proton_color.dark_blue_accent", true);
// pref("userContent.page.proton_color.system_accent",    true);
// pref("userContent.page.monospace",                     true);

// == Theme Default Settings ===================================================
pref("userChrome.compatibility.accent_color", true, locked); // ESR102 Compatibility Options
// -- User Chrome --------------------------------------------------------------
pref("userChrome.compatibility.theme",       true);
pref("userChrome.compatibility.os",          true);

pref("userChrome.theme.built_in_contrast",   true);
pref("userChrome.theme.system_default",      true);
pref("userChrome.theme.proton_color",        true);
pref("userChrome.theme.proton_chrome",       true); // Need proton_color
pref("userChrome.theme.fully_color",         true); // Need proton_color
pref("userChrome.theme.fully_dark",          true); // Need proton_color

pref("userChrome.decoration.cursor",         true);
pref("userChrome.decoration.field_border",   true);
pref("userChrome.decoration.download_panel", true);
pref("userChrome.decoration.animate",        true);

pref("userChrome.padding.tabbar_width",      true);
pref("userChrome.padding.tabbar_height",     true);
pref("userChrome.padding.toolbar_button",    true);
pref("userChrome.padding.navbar_width",      true);
pref("userChrome.padding.urlbar",            true);
pref("userChrome.padding.bookmarkbar",       true);
pref("userChrome.padding.infobar",           true);
pref("userChrome.padding.menu",              true);
pref("userChrome.padding.bookmark_menu",     true);
pref("userChrome.padding.global_menubar",    true);
pref("userChrome.padding.panel",             true);
pref("userChrome.padding.popup_panel",       true);

pref("userChrome.tab.multi_selected",        true);
pref("userChrome.tab.unloaded",              true);
pref("userChrome.tab.letters_cleary",        true);
pref("userChrome.tab.close_button_at_hover", true);
pref("userChrome.tab.sound_hide_label",      true);
pref("userChrome.tab.sound_with_favicons",   true);
pref("userChrome.tab.pip",                   true);
pref("userChrome.tab.container",             true);
pref("userChrome.tab.crashed",               true);

pref("userChrome.fullscreen.overlap",        true);
pref("userChrome.fullscreen.show_bookmarkbar", true);

pref("userChrome.icon.library",              true);
pref("userChrome.icon.panel",                true);
pref("userChrome.icon.menu",                 true);
pref("userChrome.icon.context_menu",         true);
pref("userChrome.icon.global_menu",          true);
pref("userChrome.icon.global_menubar",       true);

// -- User Content -------------------------------------------------------------
pref("userContent.player.ui",             true);
pref("userContent.player.icon",           true);
pref("userContent.player.noaudio",        true);
pref("userContent.player.size",           true);
pref("userContent.player.click_to_play",  true);
pref("userContent.player.animate",        true);

pref("userContent.newTab.field_border",   true);
pref("userContent.newTab.full_icon",      true);
pref("userContent.newTab.animate",        true);
pref("userContent.newTab.pocket_to_last", true);
pref("userContent.newTab.searchbar",      true);

pref("userContent.page.illustration",     true);
pref("userContent.page.proton_color",     true);
pref("userContent.page.dark_mode",        true); // Need proton_color
pref("userContent.page.proton",           true); // Need proton_color

// ** Useful Options ***********************************************************
// Integrated calculator at urlbar
pref("browser.urlbar.suggest.calculator", true);
