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
pref("floorp.bookmarks.bar.focus.mode", false);//フォーカスした際にブックマークバーを展開
pref("floorp.material.effect.enable", false);//マテリアルモードの実装
pref("floorp.disable.fullscreen.notification", false);//フルスクリーン通知を無効化
pref("floorp.navbar.bottom", false);
pref("floorp.tabs.showPinnedTabsTitle", false); //ピン留めされたタブのタイトルを表示
pref("floorp.delete.browser.border", false); //ブラウザーの枠線削除＆丸くする

//Fluerial UIの設定
pref("floorp.fluerial.roundVerticalTabs",false); //垂直タブのタブの形 trueが横に引っ付けるやつ、falseは角丸長方形

pref("floorp.browser.user.interface", 3);// Floorp 10 系以降のインターフェーステーマ設定
pref("floorp.browser.tabbar.settings", 0);// タブの設定
pref("floorp.tabscroll.reverse",false);
pref("floorp.tabscroll.wrap",false);

pref("floorp.enable.auto.restart", false);

pref("floorp.browser.rest.mode", false);// 休止モード


pref("browser.disable.nt.image.gb", false);// 画像を表示しない

pref("floorp.enable.dualtheme", false); //デュアルテーマの有効・無効 
pref("floorp.dualtheme.theme", "[]"); //デュアルテーマのリスト

pref("floorp.download.notification", 4); //ダウンロード通知


//新しいタブの検索を「ウェブを検索」に変更
pref("browser.newtabpage.activity-stream.improvesearch.handoffToAwesomebar", false);
//新しいタブの背景の設定
pref("browser.newtabpage.activity-stream.floorp.background.type", 1);
pref("browser.newtabpage.activity-stream.floorp.background.images.folder", "");
pref("browser.newtabpage.activity-stream.floorp.background.images.extensions", "png,jpg,jpeg,webp,gif,svg,tiff,tif,bmp,avif,jxl");
pref("browser.newtabpage.activity-stream.floorp.newtab.backdrop.blur.disable",false);

pref("floorp.multitab.bottommode", false);

pref("browser.display.statusbar", false);


pref("floorp.browser.sidebar.right", true);// サイドバーの右側を表示
pref("floorp.browser.sidebar.enable", true);// サイドバーを表示

// url:URL width:幅 userAgent:userAgent usercontext:コンテナタブ
pref("floorp.browser.sidebar2.data", '{"data":{},"index":[]}');
pref("floorp.extensions.webextensions.sidebar-action", '{"data":{}}');
pref("floorp.browser.sidebar2.addons.enabled", false);
pref("floorp.browser.sidebar2.start.url", "");
pref("floorp.browser.sidebar2.hide.to.unload.panel.enabled", false);

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
pref("floorp.tabbar.style",0);
pref("floorp.browser.tabs.verticaltab", false);
pref("floorp.enable.multitab", false);
pref("floorp.browser.tabbar.multirow.max.enabled", true);
pref("floorp.browser.tabbar.multirow.newtab-inside.enabled", false);
pref("floorp.browser.tabbar.multirow.max.row", 3);

pref("floorp.display.toolbarbutton.label", false);

// 1つ目はタイトル、2つ目は内容

// Floorp Notes
pref("floorp.browser.note.memos", "");
pref("floorp.browser.note.memos.using", -1);
pref("services.sync.prefs.sync.floorp.browser.note.memos", true);
pref("floorp.browser.note.enabled", false);

//Clock
pref("floorp.browser.clock.enabled", false);

//新規タブの開く位置
pref("floorp.browser.tabs.openNewTabPosition", -1);

//ネイティブ実装垂直タブ
pref("floorp.browser.native.verticaltabs.enabled", false);
pref("floorp.verticaltab.hover.enabled", false);

// Chrome 形式のダウンローダー
pref("floorp.browser.native.downloadbar.enabled", false);

//ワークスペース
pref("floorp.browser.workspace.tabs.state", "[]");
pref("floorp.browser.workspace.current", "");
pref("floorp.browser.workspace.all", "");
pref("floorp.browser.workspace.tab.enabled",true);
pref("floorp.browser.workspace.closePopupAfterClick", false);
pref("floorp.browser.workspace.info", "[]");
pref("floorp.browser.workspace.changeWorkspaceWithDefaultKey", true);
pref("floorp.browser.workspace.manageOnBMS", false);
pref("floorp.browser.workspace.showWorkspaceName", true);
pref("floorp.browser.workspace.backuped", false);
//temp
pref("floorp.browser.workspaces.disabledBySystem", true);

//タブバーの背景色
pref("floorp.titlebar.favicon.color", false);

// カスタムショートカットキー
pref("floorp.custom.shortcutkeysAndActions", "[]");
pref("floorp.custom.shortcutkeysAndActions.enabled", true);
pref("floorp.custom.shortcutkeysAndActions.remove.fx.actions", false);

// Profile Manager
pref("floorp.browser.profile-manager.enabled", true);

// [実験] 新しいタブのオーバーライド
pref("floorp.newtab.overrides.newtaburl", "");

//Portable
pref("floorp.portable.isUpdate", false);

pref("floorp.privateContainer.enabled", true);

/*----------------------------------------------------------------------------------------------------------------------------------*/

//ブックマークツールバー
pref("browser.toolbars.bookmarks.visibility", "always");

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

pref("toolkit.legacyUserProfileCustomizations.stylesheets" ,true);

pref("browser.preferences.moreFromMozilla", false, locked);

//たくさん閉じようとしたときに警告
pref("browser.tabs.warnOnClose", true);
pref("browser.tabs.warnOnCloseOtherTabs", true);

//addon推奨プロンプトを消す
pref("extensions.getAddons.showPane", false);

//調査と思われるものを削除。Torでは削除済み。
pref("app.normandy.api_url", "");
pref("app.normandy.enabled", true);

//SVG avif jxl 画像ファイルをの互換性向上または、既定で開けるように
pref("svg.context-properties.content.enabled", true, locked);
pref("image.avif.enabled", true, locked);
pref("image.jxl.enabled", true, locked);

// Add-On のブラックリストをFloorpが参照する際の情報漏洩削減
pref("extensions.blocklist.url", "https://blocklist.addons.mozilla.org/blocklist/3/%APP_ID%/%APP_VERSION%/");

//ブラックリストの参照の有効化
pref("extensions.blocklist.enabled", true);
pref("services.blocklist.update_enabled",	true);

//Pocket機能を無効化*/
pref("extensions.pocket.enabled", false);

//クラッシュレポートを無効化
pref("breakpad.reportURL", "", locked);
pref("browser.tabs.crashReporting.sendReport", false);
pref("browser.crashReports.unsubmittedCheck.enabled",	false);

//野良アドオンのインストールを許可。
pref("xpinstall.signatures.required", false);

// Firefox による、Mozilla へのテレメトリー送信を無効化
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

//http 通信時、Floorp は絶対にhttp:// をURLバーから隠しません
pref("browser.urlbar.trimURLs", false);

//プライバシー機能をオンにし、テレメトリー採取を無効化します。
pref("privacy.trackingprotection.origin_telemetry.enabled", false, locked);
pref("privacy.userContext.enabled", true);
pref("privacy.userContext.ui.enabled", true);
pref("trailhead.firstrun.branches", "", locked);
pref("extensions.webcompat-reporter.enabled", false);

pref("browser.startup.page", 3);//自動復元
pref("browser.tabs.closeWindowWithLastTab", false);//最後のタブを閉じてもブラウザが閉じないように

// https://developer.mozilla.org/docs/Web/API/Navigator/share
#ifdef XP_WIN
pref("dom.webshare.enabled", true);
#endif

// 開発者ツールの位置を「右」に変更
pref("devtools.toolbox.host", "right");

// user.js
pref("floorp.user.js.customize", "");
/*-----------------------------------------------------------------------------------all.js の設定-----------------------------------------------------------------------------------*/

pref("extensions.htmlaboutaddons.recommendations.enabled", false, locked);
pref("datareporting.policy.dataSubmissionEnable", false, locked);
pref("datareporting.healthreport.uploadEnabled", false, locked);
pref("toolkit.legacyUserProfileCustomizations.script", false);

/*-----------------------------------------------------------------------------以下、Photon の既定の設定-----------------------------------------------------------------------------*/
//Floorp

// 1 = photon, 2 = lepton, 3 = proton fix
pref("floorp.lepton.interface", 2);

// ** Theme Default Options ****************************************************
// userchrome.css usercontent.css activate
pref("toolkit.legacyUserProfileCustomizations.stylesheets", true);

// Proton Enabled #127 || Removed at 97 #328 (Maintained for compatibility with ESR)
pref("browser.proton.enabled", true);

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

pref("userChrome.tab.lepton_like_padding",        true); // Original
pref("userChrome.tab.photon_like_padding",       false); // Photon

pref("userChrome.tab.dynamic_separator",          true); // Original, Proton
pref("userChrome.tab.static_separator",          false); // Photon
pref("userChrome.tab.static_separator.selected_accent", false); // Just option
pref("userChrome.tab.bar_separator",             false); // Just option

pref("userChrome.tab.newtab_button_like_tab",     true); // Original
pref("userChrome.tab.newtab_button_smaller",     false); // Photon
pref("userChrome.tab.newtab_button_proton",      false); // Proton

pref("userChrome.icon.panel_full",                true); // Original, Proton
pref("userChrome.icon.panel_photon",             false); // Photon

// Original Only
pref("userChrome.tab.box_shadow",                 false);
pref("userChrome.tab.bottom_rounded_corner",      true);

// Photon Only
pref("userChrome.tab.photon_like_contextline",   false);
pref("userChrome.rounding.square_tab",           false);

// == Theme Compatibility Settings =============================================
// pref("userChrome.compatibility.accent_color",         true); // Firefox v103 Below
// pref("userChrome.compatibility.covered_header_image", true);
// pref("userChrome.compatibility.panel_cutoff",         true);
// pref("userChrome.compatibility.navbar_top_border",    true);
// pref("userChrome.compatibility.dynamic_separator",    true); // Need dynamic_separator

// pref("userChrome.compatibility.os.linux_non_native_titlebar_button", true);
// pref("userChrome.compatibility.os.windows_maximized", true);
// pref("userChrome.compatibility.os.win11",             true);

// == Theme Custom Settings ====================================================
// -- User Chrome --------------------------------------------------------------
// pref("userChrome.theme.private",                       true);
// pref("userChrome.theme.proton_color.dark_blue_accent", true);
// pref("userChrome.theme.monospace",                     true);
// pref("userChrome.theme.transparent.frame",             true);
// pref("userChrome.theme.transparent.menu",              true);
// pref("userChrome.theme.transparent.panel",             true);
// pref("userChrome.theme.non_native_menu",               true); // only for linux

// pref("userChrome.decoration.disable_panel_animate",    true);
// pref("userChrome.decoration.disable_sidebar_animate",  true);
// pref("userChrome.decoration.panel_button_separator",   true);
// pref("userChrome.decoration.panel_arrow",              true);

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
// pref("userChrome.hidden.private_indicator",            true);
// pref("userChrome.hidden.titlebar_container",           true);
// pref("userChrome.hidden.sidebar_header",               true);
// pref("userChrome.hidden.sidebar_header.vertical_tab_only", true);
// pref("userChrome.hidden.urlbar_iconbox",               true);
// pref("userChrome.hidden.urlbar_iconbox.label_only",    true);
// pref("userChrome.hidden.bookmarkbar_icon",             true);
// pref("userChrome.hidden.bookmarkbar_label",            true);
// pref("userChrome.hidden.disabled_menu",                true);

// pref("userChrome.centered.tab",                        true);
// pref("userChrome.centered.tab.label",                  true);
// pref("userChrome.centered.urlbar",                     true);
// pref("userChrome.centered.bookmarkbar",                true);

// pref("userChrome.counter.tab",                         true);
// pref("userChrome.counter.bookmark_menu",               true);

// pref("userChrome.combined.nav_button",                 true);
// pref("userChrome.combined.nav_button.home_button",     true);
// pref("userChrome.combined.urlbar.nav_button",          true);
// pref("userChrome.combined.urlbar.home_button",         true);
// pref("userChrome.combined.urlbar.reload_button",       true);
// pref("userChrome.combined.sub_button.none_background", true);
// pref("userChrome.combined.sub_button.as_normal",       true);

// pref("userChrome.rounding.square_button",              true);
// pref("userChrome.rounding.square_dialog",              true);
// pref("userChrome.rounding.square_panel",               true);
// pref("userChrome.rounding.square_panelitem",           true);
// pref("userChrome.rounding.square_menupopup",           true);
// pref("userChrome.rounding.square_menuitem",            true);
// pref("userChrome.rounding.square_infobox",             true);
// pref("userChrome.rounding.square_toolbar",             true);
// pref("userChrome.rounding.square_field",               true);
// pref("userChrome.rounding.square_urlView_item",        true);
// pref("userChrome.rounding.square_checklabel",          true);

// pref("userChrome.padding.first_tab",                   true);
// pref("userChrome.padding.first_tab.always",            true);
// pref("userChrome.padding.drag_space",                  true);
// pref("userChrome.padding.drag_space.maximized",        true);

// pref("userChrome.padding.toolbar_button.compact",      true);
// pref("userChrome.padding.menu_compact",                true);
// pref("userChrome.padding.bookmark_menu.compact",       true);
// pref("userChrome.padding.urlView_expanding",           true);
// pref("userChrome.padding.urlView_result",              true);
// pref("userChrome.padding.panel_header",                true);

// pref("userChrome.urlbar.iconbox_with_separator",       true);

// pref("userChrome.urlView.as_commandbar",               true);
// pref("userChrome.urlView.full_width_padding",          true);
// pref("userChrome.urlView.always_show_page_actions",    true);
// pref("userChrome.urlView.move_icon_to_left",           true);
// pref("userChrome.urlView.go_button_when_typing",       true);
// pref("userChrome.urlView.focus_item_border",           true);

// pref("userChrome.tabbar.as_titlebar",                  true);
// pref("userChrome.tabbar.fill_width",                   true);
// pref("userChrome.tabbar.multi_row",                    true);
// pref("userChrome.tabbar.unscroll",                     true);
// pref("userChrome.tabbar.on_bottom",                    true);
// pref("userChrome.tabbar.on_bottom.above_bookmark",     true); // Need on_bottom
// pref("userChrome.tabbar.on_bottom.menubar_on_top",     true); // Need on_bottom
// pref("userChrome.tabbar.on_bottom.hidden_single_tab",  true); // Need on_bottom
// pref("userChrome.tabbar.one_liner",                    true);
// pref("userChrome.tabbar.one_liner.combine_navbar",     true); // Need one_liner
// pref("userChrome.tabbar.one_liner.tabbar_first",       true); // Need one_liner
// pref("userChrome.tabbar.one_liner.responsive",         true); // Need one_liner

// pref("userChrome.tab.bottom_rounded_corner.all",       true);
// pref("userChrome.tab.bottom_rounded_corner.australis", true);
// pref("userChrome.tab.bottom_rounded_corner.edge",      true);
// pref("userChrome.tab.bottom_rounded_corner.chrome",    true);
// pref("userChrome.tab.bottom_rounded_corner.chrome_legacy", true);
// pref("userChrome.tab.bottom_rounded_corner.wave",      true);
// pref("userChrome.tab.always_show_tab_icon",            true);
// pref("userChrome.tab.close_button_at_pinned",          true);
// pref("userChrome.tab.close_button_at_pinned.always",   true);
// pref("userChrome.tab.close_button_at_pinned.background", true);
// pref("userChrome.tab.close_button_at_hover.always",    true); // Need close_button_at_hover
// pref("userChrome.tab.close_button_at_hover.with_selected", true);  // Need close_button_at_hover
// pref("userChrome.tab.sound_show_label",                true); // Need remove sound_hide_label
// pref("userChrome.tab.container.on_top",                true);
// pref("userChrome.tab.sound_with_favicons.on_center",   true);
// pref("userChrome.tab.selected_bold",                   true);

// pref("userChrome.navbar.as_sidebar",                   true);

// pref("userChrome.bookmarkbar.multi_row",               true);

// pref("userChrome.findbar.floating_on_top",             true);

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

// pref("userContent.newTab.hidden_logo",                 true);
// pref("userContent.newTab.background_image",            true); // Need wallpaper image --uc-newTab-wallpaper: url("../icons/background_image.png");

// pref("userContent.page.proton_color.dark_blue_accent", true);
// pref("userContent.page.proton_color.system_accent",    true);
// pref("userContent.page.dark_mode.pdf",                 true);
// pref("userContent.page.monospace",                     true);

// == Theme Default Settings ===================================================
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

pref("userContent.newTab.full_icon",      true);
pref("userContent.newTab.animate",        true);
pref("userContent.newTab.pocket_to_last", true);
pref("userContent.newTab.searchbar",      true);

pref("userContent.page.field_border",     true);
pref("userContent.page.illustration",     true);
pref("userContent.page.proton_color",     true);
pref("userContent.page.dark_mode",        true); // Need proton_color
pref("userContent.page.proton",           true); // Need proton_color

// ** Useful Options ***********************************************************
// Integrated calculator at urlbar
pref("browser.urlbar.suggest.calculator", true);

// Integrated unit convertor at urlbar
// pref("browser.urlbar.unitConversion.enabled", true);

// Draw in Titlebar
// pref("browser.tabs.drawInTitlebar", true);
// pref("browser.tabs.inTitlebar",        1); // Nightly, 96 Above

// ** Scrolling Settings *******************************************************
// == Only Sharpen Scrolling ===================================================
//         Pref                                             Value      Original
/*
pref("mousewheel.min_line_scroll_amount",                 10); //        5
pref("general.smoothScroll.mouseWheel.durationMinMS",     80); //       50
pref("general.smoothScroll.currentVelocityWeighting", "0.15"); //   "0.25"
pref("general.smoothScroll.stopDecelerationWeighting", "0.6"); //    "0.4"
*/

// == Smooth Scrolling ==========================================================
// ** Scrolling Options ********************************************************
// based on natural smooth scrolling v2 by aveyo
// this preset will reset couple extra variables for consistency
//         Pref                                              Value                 Original
/*
pref("apz.allow_zooming",                               true);            ///     true
pref("apz.force_disable_desktop_zooming_scrollbars",   false);            ///    false
pref("apz.paint_skipping.enabled",                      true);            ///     true
pref("apz.windows.use_direct_manipulation",             true);            ///     true
pref("dom.event.wheel-deltaMode-lines.always-disabled", true);            ///    false
pref("general.smoothScroll.currentVelocityWeighting", "0.12");            ///   "0.25" <- 1. If scroll too slow, set to "0.15"
pref("general.smoothScroll.durationToIntervalRatio",    1000);            ///      200
pref("general.smoothScroll.lines.durationMaxMS",         100);            ///      150
pref("general.smoothScroll.lines.durationMinMS",           0);            ///      150
pref("general.smoothScroll.mouseWheel.durationMaxMS",    100);            ///      200
pref("general.smoothScroll.mouseWheel.durationMinMS",      0);            ///       50
pref("general.smoothScroll.mouseWheel.migrationPercent", 100);            ///      100
pref("general.smoothScroll.msdPhysics.continuousMotionMaxDeltaMS", 12);   ///      120
pref("general.smoothScroll.msdPhysics.enabled",                  true);   ///    false
pref("general.smoothScroll.msdPhysics.motionBeginSpringConstant", 200);   ///     1250
pref("general.smoothScroll.msdPhysics.regularSpringConstant",     200);   ///     1000
pref("general.smoothScroll.msdPhysics.slowdownMinDeltaMS",         10);   ///       12
pref("general.smoothScroll.msdPhysics.slowdownMinDeltaRatio",  "1.20");   ///    "1.3"
pref("general.smoothScroll.msdPhysics.slowdownSpringConstant",   1000);   ///     2000
pref("general.smoothScroll.other.durationMaxMS",         100);            ///      150
pref("general.smoothScroll.other.durationMinMS",           0);            ///      150
pref("general.smoothScroll.pages.durationMaxMS",         100);            ///      150
pref("general.smoothScroll.pages.durationMinMS",           0);            ///      150
pref("general.smoothScroll.pixels.durationMaxMS",        100);            ///      150
pref("general.smoothScroll.pixels.durationMinMS",          0);            ///      150
pref("general.smoothScroll.scrollbars.durationMaxMS",    100);            ///      150
pref("general.smoothScroll.scrollbars.durationMinMS",      0);            ///      150
pref("general.smoothScroll.stopDecelerationWeighting", "0.6");            ///    "0.4"
pref("layers.async-pan-zoom.enabled",                   true);            ///     true
pref("layout.css.scroll-behavior.spring-constant",   "250.0");            ///   "250.0"
pref("mousewheel.acceleration.factor",                     3);            ///       10
pref("mousewheel.acceleration.start",                     -1);            ///       -1
pref("mousewheel.default.delta_multiplier_x",            100);            ///      100
pref("mousewheel.default.delta_multiplier_y",            100);            ///      100
pref("mousewheel.default.delta_multiplier_z",            100);            ///      100
pref("mousewheel.min_line_scroll_amount",                  0);            ///        5
pref("mousewheel.system_scroll_override.enabled",       true);            ///     true <- 2. If scroll too fast, set to false
pref("mousewheel.system_scroll_override_on_root_content.enabled", false); ///     true
pref("mousewheel.transaction.timeout",                  1500);            ///     1500
pref("toolkit.scrollbox.horizontalScrollDistance",         4);            ///        5
pref("toolkit.scrollbox.verticalScrollDistance",           3);            ///        3
*/
