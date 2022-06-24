/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser, module, require */

// This is a hack for the tests.
if (typeof InterventionHelpers === "undefined") {
  var InterventionHelpers = require("../lib/intervention_helpers");
}
if (typeof UAHelpers === "undefined") {
  var UAHelpers = require("../lib/ua_helpers");
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
     * Bug 1577519 - directv.com - Create a UA override for directv.com for playback on desktop
     * WebCompat issue #3846 - https://webcompat.com/issues/3846
     *
     * directv.com (attwatchtv.com) is blocking Firefox via UA sniffing. Spoofing as Chrome allows
     * to access the site and playback works fine. This is former directvnow.com
     */
    id: "bug1577519",
    platform: "desktop",
    domain: "directv.com",
    bug: "1577519",
    config: {
      matches: ["*://*.directv.com/*", "*://*.attwatchtv.com/*"],
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
      ],
      uaTransformer: originalUA => {
        // The first line related to Firefox 100 is for Bug 1743445.
        // [TODO]: Remove when bug 1743429 gets backed out.
        return UAHelpers.capVersionTo99(originalUA).replace(
          "Gecko",
          "like Gecko"
        );
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
  {
    /*
     * Bug 1712807 - Add UA override for www.dealnews.com
     * Webcompat issue #39341 - https://webcompat.com/issues/39341
     *
     * The sites shows Firefox a different layout compared to Chrome.
     * Spoofing as Chrome fixes this.
     */
    id: "bug1712807",
    platform: "android",
    domain: "www.dealnews.com",
    bug: "1712807",
    config: {
      matches: ["*://www.dealnews.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1719841 - Add UA override for appmedia.jp
     * Webcompat issue #78939 - https://webcompat.com/issues/78939
     *
     * The sites shows Firefox a desktop version. With Chrome's UA string,
     * we see a working mobile layout.
     */
    id: "bug1719841",
    platform: "android",
    domain: "appmedia.jp",
    bug: "1719841",
    config: {
      matches: ["*://appmedia.jp/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1719846 - Add UA override for https://covid.cdc.gov/covid-data-tracker/
     * Webcompat issue #76944 - https://webcompat.com/issues/76944
     *
     * The application locks out Firefox via User Agent sniffing, but in our
     * tests, there appears to be no reason for this. Everything looks fine if
     * we spoof as Chrome.
     */
    id: "bug1719846",
    platform: "all",
    domain: "covid.cdc.gov",
    bug: "1719846",
    config: {
      matches: ["*://covid.cdc.gov/covid-data-tracker/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1719859 - Add UA override for saxoinvestor.fr
     * Webcompat issue #74678 - https://webcompat.com/issues/74678
     *
     * The site blocks Firefox with a server-side UA sniffer. Appending a
     * Chrome version segment to the UA makes it work.
     */
    id: "bug1719859",
    platform: "all",
    domain: "saxoinvestor.fr",
    bug: "1719859",
    config: {
      matches: ["*://*.saxoinvestor.fr/*"],
      uaTransformer: originalUA => {
        return originalUA + " Chrome/91.0.4472.114";
      },
    },
  },
  {
    /*
     * Bug 1722954 - Add UA override for game.granbluefantasy.jp
     * Webcompat issue #34310 - https://github.com/webcompat/web-bugs/issues/34310
     *
     * The website is sending a version of the site which is too small. Adding a partial
     * safari iOS version of the UA sends us the right layout.
     */
    id: "bug1722954",
    platform: "android",
    domain: "granbluefantasy.jp",
    bug: "1722954",
    config: {
      matches: ["*://*.granbluefantasy.jp/*"],
      uaTransformer: originalUA => {
        return originalUA + " iPhone OS 12_0 like Mac OS X";
      },
    },
  },
  {
    /*
     * Bug 1738317 - Add UA override for vmos.cn
     * Webcompat issue #90432 - https://github.com/webcompat/web-bugs/issues/90432
     *
     * Firefox for Android receives a desktop-only layout based on server-side
     * UA sniffing. Spoofing as Chrome works fine.
     */
    id: "bug1738317",
    platform: "android",
    domain: "vmos.cn",
    bug: "1738317",
    config: {
      matches: ["*://*.vmos.cn/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1738319 - Add UA override for yebocasino.co.za
     * Webcompat issue #88409 - https://github.com/webcompat/web-bugs/issues/88409
     *
     * Firefox for Android is locked out with a "Browser Unsupported" message.
     * Spoofing as Chrome gets rid of that.
     */
    id: "bug1738319",
    platform: "android",
    domain: "yebocasino.co.za",
    bug: "1738319",
    config: {
      matches: ["*://*.yebocasino.co.za/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1743627 - Add UA override for renaud-bray.com
     * Webcompat issue #55276 - https://github.com/webcompat/web-bugs/issues/55276
     *
     * Firefox for Android depends on "Version/" being there in the UA string,
     * or it'll throw a runtime error.
     */
    id: "bug1743627",
    platform: "android",
    domain: "renaud-bray.com",
    bug: "1743627",
    config: {
      matches: ["*://*.renaud-bray.com/*"],
      uaTransformer: originalUA => {
        return originalUA + " Version/0";
      },
    },
  },
  {
    /*
     * Bug 1743745 - Add UA override for www.automesseweb.jp
     * Webcompat issue #70386 - https://github.com/webcompat/web-bugs/issues/70386
     *
     * On Firefox Android, the browser is receiving the desktop layout.
     *
     */
    id: "bug1743745",
    platform: "android",
    domain: "automesseweb.jp",
    bug: "1743745",
    config: {
      matches: ["*://*.automesseweb.jp/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1743751 - Add UA override for slrclub.com
     * Webcompat issue #91373 - https://github.com/webcompat/web-bugs/issues/91373
     *
     * On Firefox Android, the browser is receiving the desktop layout.
     * Spoofing as Chrome works fine.
     */
    id: "bug1743751",
    platform: "android",
    domain: "slrclub.com",
    bug: "1743751",
    config: {
      matches: ["*://*.slrclub.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1743754 - Add UA override for slrclub.com
     * Webcompat issue #86839 - https://github.com/webcompat/web-bugs/issues/86839
     *
     * On Firefox Android, the browser is failing a UA parsing on Firefox UA.
     */
    id: "bug1743754",
    platform: "android",
    domain: "workflow.base.vn",
    bug: "1743754",
    config: {
      matches: ["*://workflow.base.vn/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1743429 - Add UA override for sites broken with the Version 100 User Agent
     *
     * Some sites have issues with a UA string with Firefox version 100 or higher,
     * so present as version 99 for now.
     */
    id: "bug1743429",
    platform: "all",
    domain: "Sites with known Version 100 User Agent breakage",
    bug: "1743429",
    config: {
      matches: [
        "*://*.commerzbank.de/*", // Bug 1767630
        "*://*.edf.com/*", // Bug 1764786
        "*://*.ibmserviceengage.com/*", // #105438
        "*://*.wordpress.org/*", // Bug 1743431
        "*://as.eservice.asus.com/*", // #104113
        "*://bethesda.net/*", // #94607,
        "*://cdn-vzn.yottaa.net/*", // Bug 1764795
        "*://dsae.co.za/*", // Bug 1765925
        "*://fpt.dfp.microsoft.com/*", // #104237
        "*://moje.pzu.pl/*", // #99772
        "*://mon.allianzbanque.fr/*", // #101074
        "*://online.citi.com/*", // #101268
        "*://simperium.com/*", // #98934
        "*://survey.sogosurvey.com/*", // Bug 1765925
        "*://ubank.com.au/*", // #104099
        "*://wifi.sncf/*", // #100194
        "*://www.accringtonobserver.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.bathchronicle.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.bedfordshirelive.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.belfastlive.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.birminghamlive.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.birminghammail.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.birminghampost.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.bristol.live/*", // Bug 1762928 (Reach Plc)
        "*://www.bristolpost.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.buckinghamshirelive.com/*", // Bug 1762928 (Reach Plc)
        "*://www.burtonmail.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.business-live.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.cambridge-news.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.cambridgeshirelive.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.cheshire-live.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.chesterchronicle.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.chroniclelive.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.corkbeo.ie/*", // Bug 1762928 (Reach Plc)
        "*://www.cornwalllive.com/*", // Bug 1762928 (Reach Plc)
        "*://www.coventrytelegraph.net/*", // Bug 1762928 (Reach Plc)
        "*://www.crewechronicle.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.croydonadvertiser.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.dailypost.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.dailyrecord.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.dailystar.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.derbytelegraph.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.devonlive.com/*", // Bug 1762928 (Reach Plc)
        "*://www.discoveryplus.in/*", // #100389
        "*://www.dublinlive.ie/*", // Bug 1762928 (Reach Plc)
        "*://www.edinburghlive.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.essexlive.news/*", // Bug 1762928 (Reach Plc)
        "*://www.eurosportplayer.com/*", // #91087
        "*://www.examiner.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.examinerlive.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.express.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.football.london/*", // Bug 1762928 (Reach Plc)
        "*://www.footballscotland.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.gazettelive.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.getbucks.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.gethampshire.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.getreading.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.getsurrey.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.getwestlondon.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.gismeteo.ru/*", // #101326
        "*://www.glasgowlive.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.gloucestershirelive.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.grimsbytelegraph.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.hampshirelive.news/*", // Bug 1762928 (Reach Plc)
        "*://www.hannaandersson.com/*", // #95003
        "*://www.hertfordshiremercury.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.hinckleytimes.net/*", // Bug 1762928 (Reach Plc)
        "*://www.hulldailymail.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.imb.com.au/*", // Bug 1762209
        "*://www.insider.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.irishmirror.ie/*", // Bug 1762928 (Reach Plc)
        "*://www.kentlive.news/*", // Bug 1762928 (Reach Plc)
        "*://www.lancs.live/*", // Bug 1762928 (Reach Plc)
        "*://www.learningants.com/*", // #104080
        "*://www.leeds-live.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.leicestermercury.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.lincolnshirelive.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.liveobserverpark.com/*", // #105244
        "*://www.liverpool.com/*", // Bug 1762928 (Reach Plc)
        "*://www.liverpoolecho.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.loughboroughecho.net/*", // Bug 1762928 (Reach Plc)
        "*://www.macclesfield-express.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.macclesfield-live.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.manchestereveningnews.co.uk/*", // #100923
        "*://www.mylondon.news/*", // Bug 1762928 (Reach Plc)
        "*://www.northantslive.news/*", // Bug 1762928 (Reach Plc)
        "*://www.nottinghampost.com/*", // Bug 1762928 (Reach Plc)
        "*://www.ok.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.plymouthherald.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.rossendalefreepress.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.rsvplive.ie/*", // Bug 1762928 (Reach Plc)
        "*://www.scotlandnow.dailyrecord.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.screwfix.com/*", // #96959
        "*://www.scunthorpetelegraph.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.services.gov.on.ca/*", // #100926
        "*://www.smsv.com.ar/*", // #90666
        "*://www.somersetlive.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.southportvisiter.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.staffordshire-live.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.stokesentinel.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.sussexlive.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.tm-awx.com/*", // Bug 1762928 (Reach Plc)
        "*://www.walesonline.co.uk/*", // Bug 1762928 (Reach Plc)
        "*://www.wharf.co.uk/*", // Bug 1762928 (Reach Plc)
      ],
      uaTransformer: originalUA => {
        return UAHelpers.capVersionTo99(originalUA);
      },
    },
  },
  {
    /*
     * Bug 1754180 - UA override for nordjyske.dk
     * Webcompat issue #94661 - https://webcompat.com/issues/94661
     *
     * The site doesn't provide a mobile layout to Firefox for Android
     * without a Chrome UA string for a high-end device.
     */
    id: "bug1754180",
    platform: "android",
    domain: "nordjyske.dk",
    bug: "1754180",
    config: {
      matches: ["*://nordjyske.dk/*"],
      uaTransformer: originalUA => {
        return UAHelpers.getDeviceAppropriateChromeUA("97.0.4692.9", "Pixel 4");
      },
    },
  },
  {
    /*
     * Bug 1753461 - UA override for serieson.naver.com
     * Webcompat issue #99993 - https://webcompat.com/issues/97298
     *
     * The site locks out Firefox users unless a Chrome UA is given,
     * and locks out Linux users as well (so we use Windows+Chrome).
     */
    id: "bug1753461",
    platform: "desktop",
    domain: "serieson.naver.com",
    bug: "1753461",
    config: {
      matches: ["*://serieson.naver.com/*"],
      uaTransformer: originalUA => {
        return "Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/97.0.4692.99 Safari/537.36";
      },
    },
  },
  {
    /*
     * Bug 1756872 - UA override for www.dolcegabbana.com
     * Webcompat issue #99993 - https://webcompat.com/issues/99993
     *
     * The site's layout is broken on Firefox for Android
     * without a full Chrome user-agent string.
     */
    id: "bug1756872",
    platform: "android",
    domain: "www.dolcegabbana.com",
    bug: "1756872",
    config: {
      matches: ["*://www.dolcegabbana.com/*"],
      uaTransformer: originalUA => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1751604 - UA override for /www.otsuka.co.jp/fib/
     *
     * The site's content is not loaded unless a Chrome UA is used.
     */
    id: "bug1751604",
    platform: "desktop",
    domain: "www.otsuka.co.jp",
    bug: "1751604",
    config: {
      matches: ["*://www.otsuka.co.jp/fib/*"],
      uaTransformer: originalUA => {
        return UAHelpers.getDeviceAppropriateChromeUA("97.0.4692.9");
      },
    },
  },
  {
    /*
     * Bug 1771200 - UA override for animalplanet.com
     * Webcompat issue #99993 - https://webcompat.com/issues/103727
     *
     * The videos are not playing and an error message is displayed
     * in Firefox for Android, but work with Chrome UA
     */
    id: "bug1771200",
    platform: "android",
    domain: "animalplanet.com",
    bug: "1771200",
    config: {
      matches: ["*://*.animalplanet.com/video/*"],
      uaTransformer: originalUA => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
];

module.exports = AVAILABLE_UA_OVERRIDES;
