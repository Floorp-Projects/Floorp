/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser, module, require */

// This is a hack for the tests.
if (typeof InterventionHelpers === "undefined") {
  var InterventionHelpers = require("../lib/intervention_helpers");
}

/**
 * For detailed information on our policies, and a documention on this format
 * and its possibilites, please check the Mozilla-Wiki at
 *
 * https://wiki.mozilla.org/Compatibility/Go_Faster_Addon/Override_Policies_and_Workflows#User_Agent_overrides
 */
const AVAILABLE_UA_OVERRIDES = [
  {
    id: "testbed-override",
    platform: "all",
    domain: "webcompat-addon-testbed.herokuapp.com",
    bug: "0000000",
    config: {
      hidden: true,
      matches: ["*://webcompat-addon-testbed.herokuapp.com/*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/54.0.2840.98 Safari/537.36 for WebCompat"
        );
      },
    },
  },
  {
    /*
     * Bug 1577519 - att.tv - Create a UA override for att.tv for playback on desktop
     * WebCompat issue #3846 - https://webcompat.com/issues/3846
     *
     * att.tv (atttvnow.com) is blocking Firefox via UA sniffing. Spoofing as Chrome allows
     * to access the site and playback works fine. This is former directvnow.com
     */
    id: "bug1577519",
    platform: "desktop",
    domain: "att.tv",
    bug: "1577519",
    config: {
      matches: ["*://*.att.tv/*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/76.0.3809.132 Safari/537.36"
        );
      },
    },
  },
  {
    /*
     * Bug 1570108 - steamcommunity.com - UA override for steamcommunity.com
     * WebCompat issue #34171 - https://webcompat.com/issues/34171
     *
     * steamcommunity.com blocks chat feature for Firefox users showing unsupported browser message.
     * When spoofing as Chrome the chat works fine
     */
    id: "bug1570108",
    platform: "desktop",
    domain: "steamcommunity.com",
    bug: "1570108",
    config: {
      matches: ["*://steamcommunity.com/chat*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/75.0.3770.142 Safari/537.36"
        );
      },
    },
  },
  {
    /*
     * Bug 1582582 - sling.com - UA override for sling.com
     * WebCompat issue #17804 - https://webcompat.com/issues/17804
     *
     * sling.com blocks Firefox users showing unsupported browser message.
     * When spoofing as Chrome playing content works fine
     */
    id: "bug1582582",
    platform: "desktop",
    domain: "sling.com",
    bug: "1582582",
    config: {
      matches: ["https://watch.sling.com/*", "https://www.sling.com/*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/76.0.3809.132 Safari/537.36"
        );
      },
    },
  },
  {
    /*
     * Bug 1610026 - www.mobilesuica.com - UA override for www.mobilesuica.com
     * WebCompat issue #4608 - https://webcompat.com/issues/4608
     *
     * mobilesuica.com showing unsupported message for Firefox users
     * Spoofing as Chrome allows to access the page
     */
    id: "bug1610026",
    platform: "all",
    domain: "www.mobilesuica.com",
    bug: "1610026",
    config: {
      matches: ["https://www.mobilesuica.com/*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/79.0.3945.88 Safari/537.36"
        );
      },
    },
  },
  {
    /*
     * Bug 945963 - tieba.baidu.com serves simplified mobile content to Firefox Android
     * additionally, Bug 1525839 for more domains
     * WebCompat issue #18455 - https://webcompat.com/issues/18455
     *
     * tieba.baidu.com and tiebac.baidu.com serve a heavily simplified and less functional
     * mobile experience to Firefox for Android users. Adding the AppleWebKit indicator
     * to the User Agent gets us the same experience.
     */
    id: "bug945963",
    platform: "android",
    domain: "tieba.baidu.com",
    bug: "945963",
    config: {
      matches: [
        "*://baike.baidu.com/*",
        "*://image.baidu.com/*",
        "*://news.baidu.com/*",
        "*://tieba.baidu.com/*",
        "*://tiebac.baidu.com/*",
        "*://wenku.baidu.com/*",
        "*://zhidao.baidu.com/*",
      ],
      uaTransformer: originalUA => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1177298 - Write UA overrides for top Japanese Sites
     * (Imported from ua-update.json.in)
     *
     * To receive the proper mobile version instead of the desktop version or
     * a lower grade mobile experience, the UA is spoofed.
     */
    id: "bug1177298-2",
    platform: "android",
    domain: "lohaco.jp",
    bug: "1177298",
    config: {
      matches: ["*://*.lohaco.jp/*"],
      uaTransformer: _ => {
        return "Mozilla/5.0 (Linux; Android 5.0.2; Galaxy Nexus Build/IMM76B) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/43.0.2357.93 Mobile Safari/537.36";
      },
    },
  },
  {
    /*
     * Bug 1177298 - Write UA overrides for top Japanese Sites
     * (Imported from ua-update.json.in)
     *
     * To receive the proper mobile version instead of the desktop version or
     * a lower grade mobile experience, the UA is spoofed.
     */
    id: "bug1177298-3",
    platform: "android",
    domain: "nhk.or.jp",
    bug: "1177298",
    config: {
      matches: ["*://*.nhk.or.jp/*"],
      uaTransformer: originalUA => {
        return originalUA + " AppleWebKit";
      },
    },
  },
  {
    /*
     * Bug 1385206 - Create UA override for rakuten.co.jp on Firefox Android
     * (Imported from ua-update.json.in)
     *
     * rakuten.co.jp serves a Desktop version if Firefox is included in the UA.
     */
    id: "bug1385206",
    platform: "android",
    domain: "rakuten.co.jp",
    bug: "1385206",
    config: {
      matches: ["*://*.rakuten.co.jp/*"],
      uaTransformer: originalUA => {
        return originalUA.replace(/Firefox.+$/, "");
      },
    },
  },
  {
    /*
     * Bug 969844 - mobile.de sends desktop site to Firefox on Android
     *
     * mobile.de sends the desktop site to Firefox Mobile.
     * Spoofing as Chrome works fine.
     */
    id: "bug969844",
    platform: "android",
    domain: "mobile.de",
    bug: "969844",
    config: {
      matches: ["*://*.mobile.de/*"],
      uaTransformer: _ => {
        return "Mozilla/5.0 (Linux; Android 6.0.1; SM-G920F Build/MMB29K) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/55.0.2883.91 Mobile Safari/537.36";
      },
    },
  },
  {
    /*
     * Bug 1509873 - zmags.com - Add UA override for secure.viewer.zmags.com
     * WebCompat issue #21576 - https://webcompat.com/issues/21576
     *
     * The zmags viewer locks out Firefox Mobile with a "Browser unsupported"
     * message, but tests showed that it works just fine with a Chrome UA.
     * Outreach attempts were unsuccessful, and as the site has a relatively
     * high rank, we alter the UA.
     */
    id: "bug1509873",
    platform: "android",
    domain: "zmags.com",
    bug: "1509873",
    config: {
      matches: ["*://*.viewer.zmags.com/*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/55.0.2883.91 Mobile Safari/537.36"
        );
      },
    },
  },
  {
    /*
     * Bug 1566253 - posts.google.com - Add UA override for posts.google.com
     * WebCompat issue #17870 - https://webcompat.com/issues/17870
     *
     * posts.google.com displaying "Your browser doesn't support this page".
     * Spoofing as Chrome works fine.
     */
    id: "bug1566253",
    platform: "android",
    domain: "posts.google.com",
    bug: "1566253",
    config: {
      matches: ["*://posts.google.com/*"],
      uaTransformer: _ => {
        return "Mozilla/5.0 (Linux; Android 6.0.1; SM-G900M) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/75.0.3770.101 Mobile Safari/537.36";
      },
    },
  },
  {
    /*
     * Bug 1574522 - UA override for enuri.com on Firefox for Android
     * WebCompat issue #37139 - https://webcompat.com/issues/37139
     *
     * enuri.com returns a different template for Firefox on Android
     * based on server side UA detection. This results in page content cut offs.
     * Spoofing as Chrome fixes the issue
     */
    id: "bug1574522",
    platform: "android",
    domain: "enuri.com",
    bug: "1574522",
    config: {
      matches: ["*://enuri.com/*"],
      uaTransformer: _ => {
        return "Mozilla/5.0 (Linux; Android 6.0.1; SM-G900M) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/76.0.3809.111 Mobile Safari/537.36";
      },
    },
  },
  {
    /*
     * Bug 1574564 - UA override for ceskatelevize.cz on Firefox for Android
     * WebCompat issue #15467 - https://webcompat.com/issues/15467
     *
     * ceskatelevize sets streamingProtocol depending on the User-Agent it sees
     * in the request headers, returning DASH for Chrome, HLS for iOS,
     * and Flash for Firefox Mobile. Since Mobile has no Flash, the video
     * doesn't work. Spoofing as Chrome makes the video play
     */
    id: "bug1574564",
    platform: "android",
    domain: "ceskatelevize.cz",
    bug: "1574564",
    config: {
      matches: ["*://*.ceskatelevize.cz/*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/76.0.3809.111 Mobile Safari/537.36"
        );
      },
    },
  },
  {
    /*
     * Bug 1577250 - UA override for homebook.pl on Firefox for Android
     * WebCompat issue #24044 - https://webcompat.com/issues/24044
     *
     * homebook.pl shows desktop site on Firefox for Android based on
     * UA detection. Spoofing as Chrome allows to get mobile site.
     */
    id: "bug1577250",
    platform: "android",
    domain: "homebook.pl",
    bug: "1577250",
    config: {
      matches: ["*://*.homebook.pl/*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/76.0.3809.111 Mobile Safari/537.36"
        );
      },
    },
  },
  {
    /*
     * Bug 1577267 - UA override for metfone.com.kh on Firefox for Android
     * WebCompat issue #16363 - https://webcompat.com/issues/16363
     *
     * metfone.com.kh has a server side UA detection which returns desktop site
     * for Firefox for Android. Spoofing as Chrome allows to receive mobile version
     */
    id: "bug1577267",
    platform: "android",
    domain: "metfone.com.kh",
    bug: "1577267",
    config: {
      matches: ["*://*.metfone.com.kh/*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/76.0.3809.111 Mobile Safari/537.36"
        );
      },
    },
  },
  {
    /*
     * Bug 1598198 - User Agent extension for Samsung's galaxy.store URLs
     *
     * Samsung's galaxy.store shortlinks are supposed to redirect to a Samsung
     * intent:// URL on Samsung devices, but to an error page on other brands.
     * As we do not provide device info in our user agent string, this check
     * fails, and even Samsung users land on an error page if they use Firefox
     * for Android.
     * This intervention adds a simple "Samsung" identifier to the User Agent
     * on only the Galaxy Store URLs if the device happens to be a Samsung.
     */
    id: "bug1598198",
    platform: "android",
    domain: "galaxy.store",
    bug: "1598198",
    config: {
      matches: [
        "*://galaxy.store/*",
        "*://dev.galaxy.store/*",
        "*://stg.galaxy.store/*",
      ],
      uaTransformer: originalUA => {
        if (!browser.systemManufacturer) {
          return originalUA;
        }

        const manufacturer = browser.systemManufacturer.getManufacturer();
        if (manufacturer && manufacturer.toLowerCase() === "samsung") {
          return originalUA.replace("Mobile;", "Mobile; Samsung;");
        }

        return originalUA;
      },
    },
  },
  {
    /*
     * Bug 1595215 - UA overrides for Uniqlo sites
     * Webcompat issue #38825 - https://webcompat.com/issues/38825
     *
     * To receive the proper mobile version instead of the desktop version or
     * avoid redirect loop, the UA is spoofed.
     */
    id: "bug1595215",
    platform: "android",
    domain: "uniqlo.com",
    bug: "1595215",
    config: {
      matches: ["*://*.uniqlo.com/*"],
      uaTransformer: originalUA => {
        return originalUA + " Mobile Safari";
      },
    },
  },
  {
    /*
     * Bug 1621065 - UA overrides for bracketchallenge.ncaa.com
     * Webcompat issue #49886 - https://webcompat.com/issues/49886
     *
     * The NCAA bracket challenge website mistakenly classifies
     * any non-Chrome browser on Android as "is_old_android". As a result,
     * a modal is shown telling them they have security flaws. We have
     * attempted to reach out for a fix (and clarification).
     */
    id: "bug1621065",
    platform: "android",
    domain: "bracketchallenge.ncaa.com",
    bug: "1621065",
    config: {
      matches: ["*://bracketchallenge.ncaa.com/*"],
      uaTransformer: originalUA => {
        return originalUA + " Chrome";
      },
    },
  },
  {
    /*
     * Bug 1622063 - UA override for wp1-ext.usps.gov
     * Webcompat issue #29867 - https://webcompat.com/issues/29867
     *
     * The Job Search site for USPS does not work for Firefox Mobile
     * browsers (a 500 is returned).
     */
    id: "bug1622063",
    platform: "android",
    domain: "wp1-ext.usps.gov",
    bug: "1622063",
    config: {
      matches: ["*://wp1-ext.usps.gov/*"],
      uaTransformer: originalUA => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1697324 - Update the override for mobile2.bmo.com
     * Previously Bug 1622081 - UA override for mobile2.bmo.com
     * Webcompat issue #45019 - https://webcompat.com/issues/45019
     *
     * Unless the UA string contains "Chrome", mobile2.bmo.com will
     * display a modal saying the browser is out-of-date.
     */
    id: "bug1697324",
    platform: "android",
    domain: "mobile2.bmo.com",
    bug: "1697324",
    config: {
      matches: ["*://mobile2.bmo.com/*"],
      uaTransformer: originalUA => {
        return originalUA + " Chrome";
      },
    },
  },
  {
    /*
     * Bug 1628455 - UA override for autotrader.ca
     * Webcompat issue #50961 - https://webcompat.com/issues/50961
     *
     * autotrader.ca is showing desktop site for Firefox on Android
     * based on server side UA detection. Spoofing as Chrome allows to
     * get mobile experience
     */
    id: "bug1628455",
    platform: "android",
    domain: "autotrader.ca",
    bug: "1628455",
    config: {
      matches: ["https://*.autotrader.ca/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1563839 - rolb.santanderbank.com - Build UA override
     * Bug 1646791 - bancosantander.es - Re-add UA override.
     * Bug 1665129 - *.gruposantander.es - Add wildcard domains.
     * WebCompat issue #33462 - https://webcompat.com/issues/33462
     * SuMo request - https://support.mozilla.org/es/questions/1291085
     *
     * santanderbank expects UA to have 'like Gecko', otherwise it runs
     * xmlDoc.onload whose support has been dropped. It results in missing labels in forms
     * and some other issues.  Adding 'like Gecko' fixes those issues.
     */
    id: "bug1646791",
    platform: "all",
    domain: "santanderbank.com",
    bug: "1646791",
    config: {
      matches: [
        "*://*.bancosantander.es/*",
        "*://*.gruposantander.es/*",
        "*://*.santander.co.uk/*",
        "*://bob.santanderbank.com/*",
        "*://rolb.santanderbank.com/*",
      ],
      uaTransformer: originalUA => {
        return originalUA.replace("Gecko", "like Gecko");
      },
    },
  },
  {
    /*
     * Bug 1651292 - UA override for www.jp.square-enix.com
     * Webcompat issue #53018 - https://webcompat.com/issues/53018
     *
     * Unless the UA string contains "Chrome 66+", a section of
     * www.jp.square-enix.com will show a never ending LOADING
     * page.
     */
    id: "bug1651292",
    platform: "android",
    domain: "www.jp.square-enix.com",
    bug: "1651292",
    config: {
      matches: ["*://www.jp.square-enix.com/music/sem/page/FF7R/ost/*"],
      uaTransformer: originalUA => {
        return originalUA + " Chrome/83";
      },
    },
  },
  {
    /*
     * Bug 1654888 - UA override for ebuyer.com
     * Webcompat issue #52463 - https://webcompat.com/issues/52463
     *
     * This site returns desktop site based on server side UA detection.
     * Spoofing as Chrome allows to get mobile experience
     */
    id: "bug1654888",
    platform: "android",
    domain: "ebuyer.com",
    bug: "1654888",
    config: {
      matches: ["*://*.ebuyer.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1666754 - Mobile UA override for lffl.org
     * Bug 1665720 - lffl.org article page takes 2x as much time to load on Moto G
     *
     * This site returns desktop site based on server side UA detection.
     * Spoofing as Chrome allows to get mobile experience
     */
    id: "bug1666754",
    platform: "android",
    domain: "lffl.org",
    bug: "1666754",
    config: {
      matches: ["*://*.lffl.org/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1679847 - Add UA override for avto.pro
     * Webcompat issue #60043 - https://webcompat.com/issues/60043
     *
     * Unless Chrome is in the UA, the site serves a desktop version
     * on catalog pages
     */
    id: "bug1679847",
    platform: "android",
    domain: "avto.pro",
    bug: "1679847",
    config: {
      matches: ["https://avto.pro/catalog/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1679869 - Add UA override for vh1.com
     * Webcompat issue #52755 - https://webcompat.com/issues/52755
     *
     * The site is not showing videos on Firefox on mobile stating
     * that android 4.4.4 and chrome browser required
     */
    id: "bug1679869",
    platform: "android",
    domain: "vh1.com",
    bug: "1679869",
    config: {
      matches: ["*://*.vh1.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1693827 - Add UA override for www.spectrum.net/voice/
     * Webcompat issue https://bugzilla.mozilla.org/show_bug.cgi?id=1546167
     *
     * The site is not allowing Firefox users to download
     * voice messages based on UA detection. Chrome UA is required.
     */
    id: "bug1693827",
    platform: "desktop",
    domain: "spectrum.net",
    bug: "1693827",
    config: {
      matches: ["*://*.spectrum.net/voice/*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/89.0.4389.90 Safari/537.36"
        );
      },
    },
  },
  {
    /*
     * Bug 1704663 - Add UA override for automotivesuperstore.com.au
     * Webcompat issue #65664 - https://webcompat.com/issues/65664
     *
     * Page jumps back to top after scrolling down due to incorrect
     * UA detection
     */
    id: "bug1704663",
    platform: "android",
    domain: "automotivesuperstore.com.au",
    bug: "1704663",
    config: {
      matches: ["*://automotivesuperstore.com.au/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1704673 - Add UA override for app.xiaomi.com
     * Webcompat issue #66163 - https://webcompat.com/issues/66163
     *
     * The page isn’t redirecting properly error message received.
     * Spoofing as Chrome makes the page load
     */
    id: "bug1704673",
    platform: "android",
    domain: "app.xiaomi.com",
    bug: "1704673",
    config: {
      matches: ["*://app.xiaomi.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
];

const UAHelpers = {
  getDeviceAppropriateChromeUA() {
    if (!UAHelpers._deviceAppropriateChromeUA) {
      const userAgent =
        typeof navigator !== "undefined" ? navigator.userAgent : "";
      const RunningFirefoxVersion = (userAgent.match(/Firefox\/([0-9.]+)/) || [
        "",
        "58.0",
      ])[1];
      const RunningAndroidVersion =
        userAgent.match(/Android\/[0-9.]+/) || "Android 6.0";
      const ChromeVersionToMimic = "76.0.3809.111";
      const ChromePhoneUA = `Mozilla/5.0 (Linux; ${RunningAndroidVersion}; Nexus 5 Build/MRA58N) FxQuantum/${RunningFirefoxVersion} AppleWebKit/537.36 (KHTML, like Gecko) Chrome/${ChromeVersionToMimic} Mobile Safari/537.36`;
      const ChromeTabletUA = `Mozilla/5.0 (Linux; ${RunningAndroidVersion}; Nexus 7 Build/JSS15Q) FxQuantum/${RunningFirefoxVersion} AppleWebKit/537.36 (KHTML, like Gecko) Chrome/${ChromeVersionToMimic} Safari/537.36`;
      const IsPhone = userAgent.includes("Mobile");
      UAHelpers._deviceAppropriateChromeUA = IsPhone
        ? ChromePhoneUA
        : ChromeTabletUA;
    }
    return UAHelpers._deviceAppropriateChromeUA;
  },
  getPrefix(originalUA) {
    return originalUA.substr(0, originalUA.indexOf(")") + 1);
  },
};

module.exports = AVAILABLE_UA_OVERRIDES;
