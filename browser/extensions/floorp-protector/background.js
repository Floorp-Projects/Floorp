(
    async() => {
    var floorpPL = await browser.aboutConfigPrefs.getPref("floorp.protection.protectlevel");
    if (floorpPL == undefined) {
        browser.aboutConfigPrefs.setCharPref("floorp.protection.protectlevel", "2");
        document.getElementById("UASelector").value = "2";
        var floorpPL = await browser.aboutConfigPrefs.getPref("floorp.protection.protectlevel");
    }
    console.log("floorp.protection.protectlevel-startevent: " + floorpPL);
})();

document.getElementById("UASelector").addEventListener("change", function (e) {
    console.log(e.currentTarget.value)
    var selected = e.currentTarget.value;
    if (selected == "0") {
        browser.aboutConfigPrefs.setCharPref("floorp.protection.protectlevel", "0");
        console.log("floorp.protection.protectlevel-selectevent: " + selected);
        FLOORP_PROT_DEFAULT();
    } else if (selected == "1") {
        browser.aboutConfigPrefs.setCharPref("floorp.protection.protectlevel", "1");
        console.log("floorp.protection.protectlevel-selectevent: " + selected);
        FLOORP_PROT_DEFAULT();
        FLOORP_PROT_LV1();
    } else if (selected == "2") {
        browser.aboutConfigPrefs.setCharPref("floorp.protection.protectlevel", "2");
        console.log("floorp.protection.protectlevel-selectevent: " + selected);
        FLOORP_PROT_DEFAULT();
        FLOORP_PROT_LV2();
    } else if (selected == "3") {
        browser.aboutConfigPrefs.setCharPref("floorp.protection.protectlevel", "3");
        console.log("floorp.protection.protectlevel-selectevent: " + selected);
        FLOORP_PROT_DEFAULT();
        FLOORP_PROT_LV3();
    } else if (selected == "4") {
        browser.aboutConfigPrefs.setCharPref("floorp.protection.protectlevel", "4");
        console.log("floorp.protection.protectlevel-selectevent: " + selected);
        FLOORP_PROT_DEFAULT();
        FLOORP_PROT_LV4();
    }
 })


window.addEventListener("load",function(){
    (async () => {
        var pref = await browser.aboutConfigPrefs.getPref("floorp.protection.protectlevel");
        if (pref == "0") {
            document.getElementById("UASelector").value = "0";
        } else if (pref == "1") {
            document.getElementById("UASelector").value = "1";
        } else if (pref == "2") {
            document.getElementById("UASelector").value = "2";
        } else if (pref == "3") {
            document.getElementById("UASelector").value = "3";
        } else if (pref == "4") {
            document.getElementById("UASelector").value = "4";
        }
        var floorpPL = await browser.aboutConfigPrefs.getPref("floorp.protection.protectlevel")
        console.log("floorp.protection.protectlevel-loadevent: " + floorpPL)
    })();
})

async function FLOORP_PROT_DEFAULT() {
    browser.aboutConfigPrefs.setBoolPref("dom.enable_performance", true);
    browser.aboutConfigPrefs.setBoolPref("dom.enable_user_timing",	true);
    browser.aboutConfigPrefs.setBoolPref("geo.enabled", true);
    browser.aboutConfigPrefs.setCharPref("geo.wifi.uri", "");
    browser.aboutConfigPrefs.setBoolPref("geo.wifi.logging.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("dom.mozTCPSocket.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("dom.netinfo.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("dom.network.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("media.peerconnection.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("media.peerconnection.ice.default_address_only",	false);
    browser.aboutConfigPrefs.setBoolPref("media.peerconnection.ice.no_host",			false);
    browser.aboutConfigPrefs.setBoolPref("media.navigator.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("media.navigator.video.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("media.getusermedia.screensharing.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("media.getusermedia.audiocapture.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("dom.battery.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("browser.send_pings.require_same_host",		false);
    browser.aboutConfigPrefs.setIntPref("dom.maxHardwareConcurrency",	2);
    browser.aboutConfigPrefs.setBoolPref("browser.search.geoSpecificDefaults", true);
    browser.aboutConfigPrefs.setBoolPref("keyword.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("browser.urlbar.trimURLs", true);
    browser.aboutConfigPrefs.setBoolPref("security.fileuri.strict_origin_policy", false);
    browser.aboutConfigPrefs.setBoolPref("media.video_stats.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("dom.ipc.plugins.reportCrashURL",			true);
    browser.aboutConfigPrefs.setCharPref("extensions.blocklist.url", "");
    browser.aboutConfigPrefs.setBoolPref("extensions.blocklist.enabled",			false);
    browser.aboutConfigPrefs.setBoolPref("services.blocklist.update_enabled",			false);
    browser.aboutConfigPrefs.setBoolPref("browser.newtabpage.activity-stream.asrouter.userprefs.cfr",	true);
    browser.aboutConfigPrefs.setBoolPref("network.allow-experiments", true);
    browser.aboutConfigPrefs.setBoolPref("browser.tabs.crashReporting.sendReport",		true);
    browser.aboutConfigPrefs.setBoolPref("browser.crashReports.unsubmittedCheck.enabled",	true);
    browser.aboutConfigPrefs.setBoolPref("privacy.trackingprotection.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("privacy.trackingprotection.pbmode.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("privacy.userContext.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("privacy.resistFingerprinting", false);
    browser.aboutConfigPrefs.setBoolPref("browser.chrome.site_icons", true);
    browser.aboutConfigPrefs.setBoolPref("dom.security.https_only_mode", false);
    browser.aboutConfigPrefs.setBoolPref("dom.security.https_only_mode_ever_enabled", false);
    browser.aboutConfigPrefs.setBoolPref("browser.privatebrowsing.autostart", false);
    browser.aboutConfigPrefs.setBoolPref("privacy.resistFingerprinting.block_mozAddonManager", false);
    console.log("Successfully reset the configuration." );
}

async function FLOORP_PROT_LV1() {
    browser.aboutConfigPrefs.setBoolPref("extensions.blocklist.enabled",			true);
    browser.aboutConfigPrefs.setBoolPref("services.blocklist.update_enabled",			true);
    browser.aboutConfigPrefs.setBoolPref("dom.security.https_only_mode", true);
    browser.aboutConfigPrefs.setBoolPref("dom.security.https_only_mode_ever_enabled", true);
    browser.aboutConfigPrefs.setCharPref("extensions.blocklist.url", "https://blocklist.addons.mozilla.org/blocklist/3/%APP_ID%/%APP_VERSION%/");
    browser.aboutConfigPrefs.setBoolPref("dom.enable_performance", false);
    browser.aboutConfigPrefs.setCharPref("geo.wifi.uri", "https://location.services.mozilla.com/v1/geolocate?key=%MOZILLA_API_KEY%");
    browser.aboutConfigPrefs.setBoolPref("geo.wifi.logging.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("dom.mozTCPSocket.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("browser.search.geoSpecificDefaults", false);
    browser.aboutConfigPrefs.setBoolPref("browser.urlbar.trimURLs", false);
    browser.aboutConfigPrefs.setBoolPref("privacy.trackingprotection.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("browser.tabs.crashReporting.sendReport",		false);
    browser.aboutConfigPrefs.setBoolPref("browser.crashReports.unsubmittedCheck.enabled",	false);
    console.log("Configuration successfully set to LV1." );
}

async function FLOORP_PROT_LV2() {
    browser.aboutConfigPrefs.setBoolPref("extensions.blocklist.enabled",			true);
    browser.aboutConfigPrefs.setBoolPref("services.blocklist.update_enabled",			true);
    browser.aboutConfigPrefs.setBoolPref("dom.security.https_only_mode", true);
    browser.aboutConfigPrefs.setBoolPref("dom.security.https_only_mode_ever_enabled", true);
    browser.aboutConfigPrefs.setCharPref("extensions.blocklist.url", "https://blocklist.addons.mozilla.org/blocklist/3/%APP_ID%/%APP_VERSION%/");
    browser.aboutConfigPrefs.setBoolPref("dom.enable_performance", false);
    browser.aboutConfigPrefs.setCharPref("geo.wifi.uri", "https://location.services.mozilla.com/v1/geolocate?key=%MOZILLA_API_KEY%");
    browser.aboutConfigPrefs.setBoolPref("geo.wifi.logging.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("dom.mozTCPSocket.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("browser.search.geoSpecificDefaults", false);
    browser.aboutConfigPrefs.setBoolPref("browser.urlbar.trimURLs", false);
    browser.aboutConfigPrefs.setBoolPref("privacy.trackingprotection.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("dom.enable_user_timing",	false);
    browser.aboutConfigPrefs.setBoolPref("geo.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("dom.netinfo.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("dom.battery.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("privacy.trackingprotection.pbmode.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("browser.newtabpage.activity-stream.asrouter.userprefs.cfr",	false);
    browser.aboutConfigPrefs.setBoolPref("media.peerconnection.ice.default_address_only",	true);
    browser.aboutConfigPrefs.setBoolPref("media.peerconnection.ice.no_host",			true);
    browser.aboutConfigPrefs.setBoolPref("browser.send_pings.require_same_host",		true);
    browser.aboutConfigPrefs.setBoolPref("security.fileuri.strict_origin_policy", true);
    browser.aboutConfigPrefs.setBoolPref("media.video_stats.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("dom.ipc.plugins.reportCrashURL",			false);
    browser.aboutConfigPrefs.setBoolPref("browser.tabs.crashReporting.sendReport",		false);
    browser.aboutConfigPrefs.setBoolPref("browser.crashReports.unsubmittedCheck.enabled",	false);
    console.log("Configuration successfully set to LV2." );
}

async function FLOORP_PROT_LV3() {
    browser.aboutConfigPrefs.setBoolPref("extensions.blocklist.enabled",			true);
    browser.aboutConfigPrefs.setBoolPref("services.blocklist.update_enabled",			true);
    browser.aboutConfigPrefs.setBoolPref("dom.security.https_only_mode", true);
    browser.aboutConfigPrefs.setBoolPref("dom.security.https_only_mode_ever_enabled", true);
    browser.aboutConfigPrefs.setCharPref("extensions.blocklist.url", "https://blocklist.addons.mozilla.org/blocklist/3/%APP_ID%/%APP_VERSION%/");
    browser.aboutConfigPrefs.setBoolPref("dom.enable_performance", false);
    browser.aboutConfigPrefs.setCharPref("geo.wifi.uri", "https://location.services.mozilla.com/v1/geolocate?key=%MOZILLA_API_KEY%");
    browser.aboutConfigPrefs.setBoolPref("geo.wifi.logging.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("dom.mozTCPSocket.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("browser.search.geoSpecificDefaults", false);
    browser.aboutConfigPrefs.setBoolPref("browser.urlbar.trimURLs", false);
    browser.aboutConfigPrefs.setBoolPref("privacy.trackingprotection.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("dom.enable_user_timing",	false);
    browser.aboutConfigPrefs.setBoolPref("geo.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("dom.netinfo.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("dom.battery.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("privacy.trackingprotection.pbmode.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("browser.newtabpage.activity-stream.asrouter.userprefs.cfr",	false);
    browser.aboutConfigPrefs.setBoolPref("media.peerconnection.ice.default_address_only",	true);
    browser.aboutConfigPrefs.setBoolPref("media.peerconnection.ice.no_host",			true);
    browser.aboutConfigPrefs.setBoolPref("browser.send_pings.require_same_host",		true);
    browser.aboutConfigPrefs.setBoolPref("keyword.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("security.fileuri.strict_origin_policy", true);
    browser.aboutConfigPrefs.setBoolPref("media.video_stats.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("dom.ipc.plugins.reportCrashURL",			false);
    browser.aboutConfigPrefs.setBoolPref("dom.network.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("media.navigator.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("media.navigator.video.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("media.getusermedia.screensharing.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("media.getusermedia.audiocapture.enabled", false);
    browser.aboutConfigPrefs.setIntPref("dom.maxHardwareConcurrency",	2);
    browser.aboutConfigPrefs.setBoolPref("privacy.userContext.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("browser.tabs.crashReporting.sendReport",		false);
    browser.aboutConfigPrefs.setBoolPref("browser.crashReports.unsubmittedCheck.enabled",	false);
    console.log("Configuration successfully set to LV3." );
}

async function FLOORP_PROT_LV4() {
    browser.aboutConfigPrefs.setBoolPref("extensions.blocklist.enabled",			true);
    browser.aboutConfigPrefs.setBoolPref("services.blocklist.update_enabled",			true);
    browser.aboutConfigPrefs.setBoolPref("dom.security.https_only_mode", true);
    browser.aboutConfigPrefs.setBoolPref("dom.security.https_only_mode_ever_enabled", true);
    browser.aboutConfigPrefs.setCharPref("extensions.blocklist.url", "https://blocklist.addons.mozilla.org/blocklist/3/%APP_ID%/%APP_VERSION%/");
    browser.aboutConfigPrefs.setBoolPref("dom.enable_performance", false);
    browser.aboutConfigPrefs.setCharPref("geo.wifi.uri", "https://location.services.mozilla.com/v1/geolocate?key=%MOZILLA_API_KEY%");
    browser.aboutConfigPrefs.setBoolPref("geo.wifi.logging.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("dom.mozTCPSocket.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("browser.search.geoSpecificDefaults", false);
    browser.aboutConfigPrefs.setBoolPref("browser.urlbar.trimURLs", false);
    browser.aboutConfigPrefs.setBoolPref("privacy.trackingprotection.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("dom.enable_user_timing",	false);
    browser.aboutConfigPrefs.setBoolPref("geo.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("dom.netinfo.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("dom.battery.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("privacy.trackingprotection.pbmode.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("browser.newtabpage.activity-stream.asrouter.userprefs.cfr",	false);
    browser.aboutConfigPrefs.setBoolPref("media.peerconnection.ice.default_address_only",	true);
    browser.aboutConfigPrefs.setBoolPref("media.peerconnection.ice.no_host",			true);
    browser.aboutConfigPrefs.setBoolPref("browser.send_pings.require_same_host",		true);
    browser.aboutConfigPrefs.setBoolPref("keyword.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("security.fileuri.strict_origin_policy", true);
    browser.aboutConfigPrefs.setBoolPref("media.video_stats.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("dom.ipc.plugins.reportCrashURL",			false);
    browser.aboutConfigPrefs.setBoolPref("privacy.resistFingerprinting", true);
    browser.aboutConfigPrefs.setBoolPref("media.peerconnection.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("media.navigator.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("media.navigator.video.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("media.getusermedia.screensharing.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("media.getusermedia.audiocapture.enabled", false);
    browser.aboutConfigPrefs.setIntPref("dom.maxHardwareConcurrency",	2);
    browser.aboutConfigPrefs.setBoolPref("privacy.userContext.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("browser.tabs.crashReporting.sendReport",		false);
    browser.aboutConfigPrefs.setBoolPref("browser.crashReports.unsubmittedCheck.enabled",	false);
    browser.aboutConfigPrefs.setBoolPref("network.allow-experiments", false);
    browser.aboutConfigPrefs.setBoolPref("browser.chrome.site_icons", false);
    browser.aboutConfigPrefs.setBoolPref("browser.privatebrowsing.autostart", true);
    browser.aboutConfigPrefs.setBoolPref("privacy.resistFingerprinting.block_mozAddonManager", true);
    console.log("Configuration successfully set to LV4." );
}

(
	function(){
    var menu_elem_content_1 = document.querySelector("#exper")
    var menu_elem_content_2 = document.querySelector("#header-main")
    var menu_elem_content_3 = document.querySelector("#info")
    var menu_elem_content_4_0 = document.querySelector("[id='0']")
    var menu_elem_content_4_1 = document.querySelector("[id='1']")
    var menu_elem_content_4_2 = document.querySelector("[id='2']")
    var menu_elem_content_4_3 = document.querySelector("[id='3']")
    var menu_elem_content_4_4 = document.querySelector("[id='4']")
    var menu_elem_content_5 = document.querySelector("#Feedback")

    menu_elem_content_1.innerText = browser.i18n.getMessage("exper")
    menu_elem_content_2.innerText = browser.i18n.getMessage("header-main")
    menu_elem_content_3.innerText = browser.i18n.getMessage("info")
    menu_elem_content_4_0.innerText = browser.i18n.getMessage("0")
    menu_elem_content_4_1.innerText = browser.i18n.getMessage("1")
    menu_elem_content_4_2.innerText = browser.i18n.getMessage("2")
    menu_elem_content_4_3.innerText = browser.i18n.getMessage("3") 
    menu_elem_content_4_4.innerText = browser.i18n.getMessage("4")
    menu_elem_content_5.innerText = browser.i18n.getMessage("Feedback") 
    
}())
/*
        //パフォーマンス観測用のAPIを遮断
    browser.aboutConfigPrefs.setBoolPref("dom.enable_performance", false);

// Timing APIが新しい高解像度タイムスタンプを提供しないように from Tor Browser
    browser.aboutConfigPrefs.setBoolPref("dom.enable_user_timing",	false);

//「既定でオンを推奨」ロケーションサービスの無効化
    browser.aboutConfigPrefs.setBoolPref("geo.enabled", false);

//「既定でオンを推奨」ロケーションサービスが有効化されている場合(8行目)、Google ではなく、Mozilla のサーバを利用します
    browser.aboutConfigPrefs.setCharPref("geo.wifi.uri", "https://location.services.mozilla.com/v1/geolocate?key=%MOZILLA_API_KEY%");

//「既定でオンを推奨」 ロケーションが有効（8行目）な場合、ロケーションのリクエストをブラウザーに記録しないように
    browser.aboutConfigPrefs.setBoolPref("geo.wifi.logging.enabled", false);

//デバイスから生のデータをそのまま送信することを禁止します
    browser.aboutConfigPrefs.setBoolPref("dom.mozTCPSocket.enabled", false);

//javaScript から、Floorp ブラウザーのネットワーク状態を読み取られるのを防止します。
    browser.aboutConfigPrefs.setBoolPref("dom.netinfo.enabled", false);

//「既定でオンを推奨」フィンガープリント対策の一環。
//参考：https://www.torproject.org/projects/torbrowser/design/#fingerprinting-defenses
    browser.aboutConfigPrefs.setBoolPref("dom.network.enabled", false);

//「既定でオンを推奨」webRTCを完全に無効化します。Brave・Safari では、既定で無効化されています。Floorp はこれによって、IP アドレスの漏洩を防ぎます。
    browser.aboutConfigPrefs.setBoolPref("media.peerconnection.enabled", false);

//WebRTC が有効な場合、Floorp はできるだけ、IPアドレスを秘匿するよう動作します。
    browser.aboutConfigPrefs.setBoolPref("media.peerconnection.ice.default_address_only",	true);
    browser.aboutConfigPrefs.setBoolPref("media.peerconnection.ice.no_host",			true);

//WebRTCのgetUserMedia、画面共有、オーディオキャプチャ、ビデオキャプチャの無効化
    browser.aboutConfigPrefs.setBoolPref("media.navigator.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("media.navigator.video.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("media.getusermedia.screensharing.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("media.getusermedia.audiocapture.enabled", false);

//ウェブサイトがバッテリーの情報を取得するのを防ぎます
    browser.aboutConfigPrefs.setBoolPref("dom.battery.enabled", false);

//サードパーティーのウェブサイト等にpingを送信することを禁止します
    browser.aboutConfigPrefs.setBoolPref("browser.send_pings.require_same_host",		true);

// CPUのコア数を偽装し、正しい値をウェブサイトに返さないようにします。
    browser.aboutConfigPrefs.setBoolPref("dom.maxHardwareConcurrency",	2);

//「既定でオンを推奨」Mozillaが提供する位置特定型検索エンジンを使用しない。位置情報がより保護されます。
    browser.aboutConfigPrefs.setBoolPref("browser.search.geoSpecificDefaults", false);

// 「既定でオンを推奨」アドレスバーに入力された無効なURLをデフォルトの検索エンジンに送信しないようにする。
    browser.aboutConfigPrefs.setBoolPref("keyword.enabled", false);

//「既定でオンを推奨」http 通信時、Floorp は絶対にhttp:// をURLバーから隠しません
    browser.aboutConfigPrefs.setBoolPref("browser.urlbar.trimURLs", false);

//「既定でオンを推奨」デバイスから読み取られたファイルがアクセスできる場所を制限します。
    browser.aboutConfigPrefs.setBoolPref("security.fileuri.strict_origin_policy", true);

//「既定でオンを推奨」 フィンガープリントの脅威を軽減する為、ビデオ統計情報を無効にする
    browser.aboutConfigPrefs.setBoolPref("media.video_stats.enabled", false);

//プラグインのクラッシュレポートを削除
    browser.aboutConfigPrefs.setBoolPref("dom.ipc.plugins.reportCrashURL",			false);

// Add-On のブラックリストをFloorpが参照する際の情報漏洩削減
    browser.aboutConfigPrefs.setCharPref("extensions.blocklist.url", "https://blocklist.addons.mozilla.org/blocklist/3/%APP_ID%/%APP_VERSION%/");

//ブラックリストの参照の有効化
    browser.aboutConfigPrefs.setBoolPref("extensions.blocklist.enabled",			true);
    browser.aboutConfigPrefs.setBoolPref("services.blocklist.update_enabled",			true);

//「既定でオンを推奨」新しいタブでおすすめのアドオンを紹介してくるやつを殺す
    browser.aboutConfigPrefs.setBoolPref("browser.newtabpage.activity-stream.asrouter.userprefs.cfr",	false);


//Tor が無効化していたので、、、なんかの実験っぽい
    browser.aboutConfigPrefs.setBoolPref("network.allow-experiments", false);

//「既定でオンを推奨」クラッシュレポートを根本から殺す
    browser.aboutConfigPrefs.setBoolPref("browser.tabs.crashReporting.sendReport",		false);
    browser.aboutConfigPrefs.setBoolPref("browser.crashReports.unsubmittedCheck.enabled",	false);

// 「既定でオンを推奨」firefox のトラッキング保護をオンに
    browser.aboutConfigPrefs.setBoolPref("privacy.trackingprotection.enabled", true);

//こっちは強力にするやつ
    browser.aboutConfigPrefs.setBoolPref("privacy.trackingprotection.pbmode.enabled", true);

//「既定でオンを推奨」プライバシーコンテナータブを有効化（詳細は不明）
    browser.aboutConfigPrefs.setBoolPref("privacy.userContext.enabled", true);

//「既定でオンを推奨」強力なフィンガープリント採取破壊を有効化します。これを有効化すると一部のウェブサイトが破損します
    browser.aboutConfigPrefs.setBoolPref("privacy.resistFingerprinting", true);

//「既定でオンを推奨」フィンガープリント対策
    browser.aboutConfigPrefs.setBoolPref("browser.chrome.site_icons", false);
*/