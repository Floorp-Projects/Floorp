/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci, Cc } = require("chrome");
const { Services } = require("resource://gre/modules/Services.jsm");
const Strings = Services.strings.createBundle("chrome://browser/locale/devtools/device.properties");

/* `Devices` is a catalog of existing devices and their properties, intended
 * for (mobile) device emulation tools and features.
 *
 * The properties of a device are:
 * - name: Device brand and model(s).
 * - width: Viewport width.
 * - height: Viewport height.
 * - pixelRatio: Screen pixel ratio to viewport.
 * - userAgent: Device UserAgent string.
 * - touch: Whether the screen is touch-enabled.
 *
 * To add more devices to this catalog, either patch this file, or push new
 * device descriptions from your own code (e.g. an addon) like so:
 *
 *   var myPhone = { name: "My Phone", ... };
 *   require("devtools/shared/devices").Devices.Others.phones.push(myPhone);
 */

let Devices = {
  Types: ["phones", "tablets", "notebooks", "televisions", "watches"],

  // Get the localized string of a device type.
  GetString(deviceType) {
    return Strings.GetStringFromName("device." + deviceType);
  },
};
exports.Devices = Devices;


// The `Devices.FirefoxOS` list was put together from various sources online.
Devices.FirefoxOS = {
  phones: [
    {
      name: "Firefox OS Flame",
      width: 320,
      height: 570,
      pixelRatio: 1.5,
      userAgent: "Mozilla/5.0 (Mobile; rv:28.0) Gecko/28.0 Firefox/28.0",
      touch: true,
    },
    {
      name: "Alcatel One Touch Fire, Fire C",
      width: 320,
      height: 480,
      pixelRatio: 1,
      userAgent: "Mozilla/5.0 (Mobile; ALCATELOneTouch4012X; rv:28.0) Gecko/28.0 Firefox/28.0",
      touch: true,
    },
    {
      name: "Alcatel Fire E",
      width: 320,
      height: 480,
      pixelRatio: 2,
      userAgent: "Mozilla/5.0 (Mobile; ALCATELOneTouch4012X; rv:28.0) Gecko/28.0 Firefox/28.0",
      touch: true,
    },
    {
      name: "Geeksphone Keon",
      width: 320,
      height: 480,
      pixelRatio: 1,
      userAgent: "Mozilla/5.0 (Mobile; rv:28.0) Gecko/28.0 Firefox/28.0",
      touch: true,
    },
    {
      name: "Geeksphone Peak, Revolution",
      width: 360,
      height: 640,
      pixelRatio: 1.5,
      userAgent: "Mozilla/5.0 (Mobile; rv:28.0) Gecko/28.0 Firefox/28.0",
      touch: true,
    },
    {
      name: "Intex Cloud Fx",
      width: 320,
      height: 480,
      pixelRatio: 1,
      userAgent: "Mozilla/5.0 (Mobile; rv:28.0) Gecko/28.0 Firefox/28.0",
      touch: true,
    },
    {
      name: "LG Fireweb",
      width: 320,
      height: 480,
      pixelRatio: 1,
      userAgent: "Mozilla/5.0 (Mobile; LG-D300; rv:28.0) Gecko/28.0 Firefox/28.0",
      touch: true,
    },
    {
      name: "Spice Fire One Mi-FX1",
      width: 320,
      height: 480,
      pixelRatio: 1,
      userAgent: "Mozilla/5.0 (Mobile; rv:28.0) Gecko/28.0 Firefox/28.0",
      touch: true,
    },
    {
      name: "Symphony GoFox F15",
      width: 320,
      height: 480,
      pixelRatio: 1,
      userAgent: "Mozilla/5.0 (Mobile; rv:28.0) Gecko/28.0 Firefox/28.0",
      touch: true,
    },
    {
      name: "Zen Fire 105",
      width: 320,
      height: 480,
      pixelRatio: 1,
      userAgent: "Mozilla/5.0 (Mobile; rv:28.0) Gecko/28.0 Firefox/28.0",
      touch: true,
    },
    {
      name: "ZTE Open",
      width: 320,
      height: 480,
      pixelRatio: 1,
      userAgent: "Mozilla/5.0 (Mobile; ZTEOPEN; rv:28.0) Gecko/28.0 Firefox/28.0",
      touch: true,
    },
    {
      name: "ZTE Open C",
      width: 320,
      height: 450,
      pixelRatio: 1.5,
      userAgent: "Mozilla/5.0 (Mobile; OPENC; rv:28.0) Gecko/28.0 Firefox/28.0",
      touch: true,
    },
  ],
  tablets: [
    {
      name: "Foxconn InFocus",
      width: 1280,
      height: 800,
      pixelRatio: 1,
      userAgent: "Mozilla/5.0 (Mobile; rv:28.0) Gecko/28.0 Firefox/28.0",
      touch: true,
    },
    {
      name: "VIA Vixen",
      width: 1024,
      height: 600,
      pixelRatio: 1,
      userAgent: "Mozilla/5.0 (Mobile; rv:28.0) Gecko/28.0 Firefox/28.0",
      touch: true,
    },
  ],
  notebooks: [
  ],
  televisions: [
    {
      name: "720p HD Television",
      width: 1280,
      height: 720,
      pixelRatio: 1,
      userAgent: "",
      touch: false,
    },
    {
      name: "1080p Full HD Television",
      width: 1920,
      height: 1080,
      pixelRatio: 1,
      userAgent: "",
      touch: false,
    },
    {
      name: "4K Ultra HD Television",
      width: 3840,
      height: 2160,
      pixelRatio: 1,
      userAgent: "",
      touch: false,
    },
  ],
  watches: [
    {
      name: "LG G Watch",
      width: 280,
      height: 280,
      pixelRatio: 1,
      userAgent: "",
      touch: true,
    },
    {
      name: "LG G Watch R",
      width: 320,
      height: 320,
      pixelRatio: 1,
      userAgent: "",
      touch: true,
    },
    {
      name: "Moto 360",
      width: 320,
      height: 290,
      pixelRatio: 1,
      userAgent: "",
      touch: true,
    },
    {
      name: "Samsung Gear Live",
      width: 320,
      height: 320,
      pixelRatio: 1,
      userAgent: "",
      touch: true,
    },
  ],
};

// `Devices.Others` was derived from the Chromium source code:
// - chromium/src/third_party/WebKit/Source/devtools/front_end/toolbox/OverridesUI.js
Devices.Others = {
  phones: [
    {
      name: "Apple iPhone 3GS",
      width: 320,
      height: 480,
      pixelRatio: 1,
      userAgent: "Mozilla/5.0 (iPhone; U; CPU iPhone OS 4_2_1 like Mac OS X; en-us) AppleWebKit/533.17.9 (KHTML, like Gecko) Version/5.0.2 Mobile/8C148 Safari/6533.18.5",
      touch: true,
    },
    {
      name: "Apple iPhone 4",
      width: 320,
      height: 480,
      pixelRatio: 2,
      userAgent: "Mozilla/5.0 (iPhone; U; CPU iPhone OS 4_2_1 like Mac OS X; en-us) AppleWebKit/533.17.9 (KHTML, like Gecko) Version/5.0.2 Mobile/8C148 Safari/6533.18.5",
      touch: true,
    },
    {
      name: "Apple iPhone 5",
      width: 320,
      height: 568,
      pixelRatio: 2,
      userAgent: "Mozilla/5.0 (iPhone; CPU iPhone OS 7_0 like Mac OS X; en-us) AppleWebKit/537.51.1 (KHTML, like Gecko) Version/7.0 Mobile/11A465 Safari/9537.53",
      touch: true,
    },
    {
      name: "Apple iPhone 6",
      width: 375,
      height: 667,
      pixelRatio: 2,
      userAgent: "Mozilla/5.0 (iPhone; CPU iPhone OS 8_0 like Mac OS X) AppleWebKit/600.1.3 (KHTML, like Gecko) Version/8.0 Mobile/12A4345d Safari/600.1.4",
      touch: true,
    },
    {
      name: "Apple iPhone 6 Plus",
      width: 414,
      height: 736,
      pixelRatio: 3,
      userAgent: "Mozilla/5.0 (iPhone; CPU iPhone OS 8_0 like Mac OS X) AppleWebKit/600.1.3 (KHTML, like Gecko) Version/8.0 Mobile/12A4345d Safari/600.1.4",
      touch: true,
    },
    {
      name: "BlackBerry Z10",
      width: 384,
      height: 640,
      pixelRatio: 2,
      userAgent: "Mozilla/5.0 (BB10; Touch) AppleWebKit/537.10+ (KHTML, like Gecko) Version/10.0.9.2372 Mobile Safari/537.10+",
      touch: true,
    },
    {
      name: "BlackBerry Z30",
      width: 360,
      height: 640,
      pixelRatio: 2,
      userAgent: "Mozilla/5.0 (BB10; Touch) AppleWebKit/537.10+ (KHTML, like Gecko) Version/10.0.9.2372 Mobile Safari/537.10+",
      touch: true,
    },
    {
      name: "Google Nexus 4",
      width: 384,
      height: 640,
      pixelRatio: 2,
      userAgent: "Mozilla/5.0 (Linux; Android 4.2.1; en-us; Nexus 4 Build/JOP40D) AppleWebKit/535.19 (KHTML, like Gecko) Chrome/18.0.1025.166 Mobile Safari/535.19",
      touch: true,
    },
    {
      name: "Google Nexus 5",
      width: 360,
      height: 640,
      pixelRatio: 3,
      userAgent: "Mozilla/5.0 (Linux; Android 4.2.1; en-us; Nexus 5 Build/JOP40D) AppleWebKit/535.19 (KHTML, like Gecko) Chrome/18.0.1025.166 Mobile Safari/535.19",
      touch: true,
    },
    {
      name: "Google Nexus S",
      width: 320,
      height: 533,
      pixelRatio: 1.5,
      userAgent: "Mozilla/5.0 (Linux; U; Android 2.3.4; en-us; Nexus S Build/GRJ22) AppleWebKit/533.1 (KHTML, like Gecko) Version/4.0 Mobile Safari/533.1",
      touch: true,
    },
    {
      name: "HTC Evo, Touch HD, Desire HD, Desire",
      width: 320,
      height: 533,
      pixelRatio: 1.5,
      userAgent: "Mozilla/5.0 (Linux; U; Android 2.2; en-us; Sprint APA9292KT Build/FRF91) AppleWebKit/533.1 (KHTML, like Gecko) Version/4.0 Mobile Safari/533.1",
      touch: true,
    },
    {
      name: "HTC One X, EVO LTE",
      width: 360,
      height: 640,
      pixelRatio: 2,
      userAgent: "Mozilla/5.0 (Linux; Android 4.0.3; HTC One X Build/IML74K) AppleWebKit/535.19 (KHTML, like Gecko) Chrome/18.0.1025.133 Mobile Safari/535.19",
      touch: true,
    },
    {
      name: "HTC Sensation, Evo 3D",
      width: 360,
      height: 640,
      pixelRatio: 1.5,
      userAgent: "Mozilla/5.0 (Linux; U; Android 4.0.3; en-us; HTC Sensation Build/IML74K) AppleWebKit/534.30 (KHTML, like Gecko) Version/4.0 Mobile Safari/534.30",
      touch: true,
    },
    {
      name: "LG Optimus 2X, Optimus 3D, Optimus Black",
      width: 320,
      height: 533,
      pixelRatio: 1.5,
      userAgent: "Mozilla/5.0 (Linux; U; Android 2.2; en-us; LG-P990/V08c Build/FRG83) AppleWebKit/533.1 (KHTML, like Gecko) Version/4.0 Mobile Safari/533.1 MMS/LG-Android-MMS-V1.0/1.2",
      touch: true,
    },
    {
      name: "LG Optimus G",
      width: 384,
      height: 640,
      pixelRatio: 2,
      userAgent: "Mozilla/5.0 (Linux; Android 4.0; LG-E975 Build/IMM76L) AppleWebKit/535.19 (KHTML, like Gecko) Chrome/18.0.1025.166 Mobile Safari/535.19",
      touch: true,
    },
    {
      name: "LG Optimus LTE, Optimus 4X HD",
      width: 424,
      height: 753,
      pixelRatio: 1.7,
      userAgent: "Mozilla/5.0 (Linux; U; Android 2.3; en-us; LG-P930 Build/GRJ90) AppleWebKit/533.1 (KHTML, like Gecko) Version/4.0 Mobile Safari/533.1",
      touch: true,
    },
    {
      name: "LG Optimus One",
      width: 213,
      height: 320,
      pixelRatio: 1.5,
      userAgent: "Mozilla/5.0 (Linux; U; Android 2.2.1; en-us; LG-MS690 Build/FRG83) AppleWebKit/533.1 (KHTML, like Gecko) Version/4.0 Mobile Safari/533.1",
      touch: true,
    },
    {
      name: "Motorola Defy, Droid, Droid X, Milestone",
      width: 320,
      height: 569,
      pixelRatio: 1.5,
      userAgent: "Mozilla/5.0 (Linux; U; Android 2.0; en-us; Milestone Build/ SHOLS_U2_01.03.1) AppleWebKit/530.17 (KHTML, like Gecko) Version/4.0 Mobile Safari/530.17",
      touch: true,
    },
    {
      name: "Motorola Droid 3, Droid 4, Droid Razr, Atrix 4G, Atrix 2",
      width: 540,
      height: 960,
      pixelRatio: 1,
      userAgent: "Mozilla/5.0 (Linux; U; Android 2.2; en-us; Droid Build/FRG22D) AppleWebKit/533.1 (KHTML, like Gecko) Version/4.0 Mobile Safari/533.1",
      touch: true,
    },
    {
      name: "Motorola Droid Razr HD",
      width: 720,
      height: 1280,
      pixelRatio: 1,
      userAgent: "Mozilla/5.0 (Linux; U; Android 2.3; en-us; DROID RAZR 4G Build/6.5.1-73_DHD-11_M1-29) AppleWebKit/533.1 (KHTML, like Gecko) Version/4.0 Mobile Safari/533.1",
      touch: true,
    },
    {
      name: "Nokia C5, C6, C7, N97, N8, X7",
      width: 360,
      height: 640,
      pixelRatio: 1,
      userAgent: "NokiaN97/21.1.107 (SymbianOS/9.4; Series60/5.0 Mozilla/5.0; Profile/MIDP-2.1 Configuration/CLDC-1.1) AppleWebkit/525 (KHTML, like Gecko) BrowserNG/7.1.4",
      touch: true,
    },
    {
      name: "Nokia Lumia 7X0, Lumia 8XX, Lumia 900, N800, N810, N900",
      width: 320,
      height: 533,
      pixelRatio: 1.5,
      userAgent: "Mozilla/5.0 (compatible; MSIE 10.0; Windows Phone 8.0; Trident/6.0; IEMobile/10.0; ARM; Touch; NOKIA; Lumia 820)",
      touch: true,
    },
    {
      name: "Samsung Galaxy Note 3",
      width: 360,
      height: 640,
      pixelRatio: 3,
      userAgent: "Mozilla/5.0 (Linux; U; Android 4.3; en-us; SM-N900T Build/JSS15J) AppleWebKit/534.30 (KHTML, like Gecko) Version/4.0 Mobile Safari/534.30",
      touch: true,
    },
    {
      name: "Samsung Galaxy Note II",
      width: 360,
      height: 640,
      pixelRatio: 2,
      userAgent: "Mozilla/5.0 (Linux; U; Android 4.1; en-us; GT-N7100 Build/JRO03C) AppleWebKit/534.30 (KHTML, like Gecko) Version/4.0 Mobile Safari/534.30",
      touch: true,
    },
    {
      name: "Samsung Galaxy Note",
      width: 400,
      height: 640,
      pixelRatio: 2,
      userAgent: "Mozilla/5.0 (Linux; U; Android 2.3; en-us; SAMSUNG-SGH-I717 Build/GINGERBREAD) AppleWebKit/533.1 (KHTML, like Gecko) Version/4.0 Mobile Safari/533.1",
      touch: true,
    },
    {
      name: "Samsung Galaxy S III, Galaxy Nexus",
      width: 360,
      height: 640,
      pixelRatio: 2,
      userAgent: "Mozilla/5.0 (Linux; U; Android 4.0; en-us; GT-I9300 Build/IMM76D) AppleWebKit/534.30 (KHTML, like Gecko) Version/4.0 Mobile Safari/534.30",
      touch: true,
    },
    {
      name: "Samsung Galaxy S, S II, W",
      width: 320,
      height: 533,
      pixelRatio: 1.5,
      userAgent: "Mozilla/5.0 (Linux; U; Android 2.1; en-us; GT-I9000 Build/ECLAIR) AppleWebKit/525.10+ (KHTML, like Gecko) Version/3.0.4 Mobile Safari/523.12.2",
      touch: true,
    },
    {
      name: "Samsung Galaxy S4",
      width: 360,
      height: 640,
      pixelRatio: 3,
      userAgent: "Mozilla/5.0 (Linux; Android 4.2.2; GT-I9505 Build/JDQ39) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/31.0.1650.59 Mobile Safari/537.36",
      touch: true,
    },
    {
      name: "Sony Xperia S, Ion",
      width: 360,
      height: 640,
      pixelRatio: 2,
      userAgent: "Mozilla/5.0 (Linux; U; Android 4.0; en-us; LT28at Build/6.1.C.1.111) AppleWebKit/534.30 (KHTML, like Gecko) Version/4.0 Mobile Safari/534.30",
      touch: true,
    },
    {
      name: "Sony Xperia Sola, U",
      width: 480,
      height: 854,
      pixelRatio: 1,
      userAgent: "Mozilla/5.0 (Linux; U; Android 2.3; en-us; SonyEricssonST25i Build/6.0.B.1.564) AppleWebKit/533.1 (KHTML, like Gecko) Version/4.0 Mobile Safari/533.1",
      touch: true,
    },
    {
      name: "Sony Xperia Z, Z1",
      width: 360,
      height: 640,
      pixelRatio: 3,
      userAgent: "Mozilla/5.0 (Linux; U; Android 4.2; en-us; SonyC6903 Build/14.1.G.1.518) AppleWebKit/534.30 (KHTML, like Gecko) Version/4.0 Mobile Safari/534.30",
      touch: true,
    },
  ],
  tablets: [
    {
      name: "Amazon Kindle Fire HDX 7″",
      width: 1920,
      height: 1200,
      pixelRatio: 2,
      userAgent: "Mozilla/5.0 (Linux; U; en-us; KFTHWI Build/JDQ39) AppleWebKit/535.19 (KHTML, like Gecko) Silk/3.13 Safari/535.19 Silk-Accelerated=true",
      touch: true,
    },
    {
      name: "Amazon Kindle Fire HDX 8.9″",
      width: 2560,
      height: 1600,
      pixelRatio: 2,
      userAgent: "Mozilla/5.0 (Linux; U; en-us; KFAPWI Build/JDQ39) AppleWebKit/535.19 (KHTML, like Gecko) Silk/3.13 Safari/535.19 Silk-Accelerated=true",
      touch: true,
    },
    {
      name: "Amazon Kindle Fire (First Generation)",
      width: 1024,
      height: 600,
      pixelRatio: 1,
      userAgent: "Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10_6_3; en-us; Silk/1.0.141.16-Gen4_11004310) AppleWebkit/533.16 (KHTML, like Gecko) Version/5.0 Safari/533.16 Silk-Accelerated=true",
      touch: true,
    },
    {
      name: "Apple iPad 1 / 2 / iPad Mini",
      width: 1024,
      height: 768,
      pixelRatio: 1,
      userAgent: "Mozilla/5.0 (iPad; CPU OS 4_3_5 like Mac OS X; en-us) AppleWebKit/533.17.9 (KHTML, like Gecko) Version/5.0.2 Mobile/8L1 Safari/6533.18.5",
      touch: true,
    },
    {
      name: "Apple iPad 3 / 4",
      width: 1024,
      height: 768,
      pixelRatio: 2,
      userAgent: "Mozilla/5.0 (iPad; CPU OS 7_0 like Mac OS X) AppleWebKit/537.51.1 (KHTML, like Gecko) Version/7.0 Mobile/11A465 Safari/9537.53",
      touch: true,
    },
    {
      name: "BlackBerry PlayBook",
      width: 1024,
      height: 600,
      pixelRatio: 1,
      userAgent: "Mozilla/5.0 (PlayBook; U; RIM Tablet OS 2.1.0; en-US) AppleWebKit/536.2+ (KHTML like Gecko) Version/7.2.1.0 Safari/536.2+",
      touch: true,
    },
    {
      name: "Google Nexus 10",
      width: 1280,
      height: 800,
      pixelRatio: 2,
      userAgent: "Mozilla/5.0 (Linux; Android 4.3; Nexus 10 Build/JSS15Q) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/29.0.1547.72 Safari/537.36",
      touch: true,
    },
    {
      name: "Google Nexus 7 2",
      width: 960,
      height: 600,
      pixelRatio: 2,
      userAgent: "Mozilla/5.0 (Linux; Android 4.3; Nexus 7 Build/JSS15Q) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/29.0.1547.72 Safari/537.36",
      touch: true,
    },
    {
      name: "Google Nexus 7",
      width: 966,
      height: 604,
      pixelRatio: 1.325,
      userAgent: "Mozilla/5.0 (Linux; Android 4.3; Nexus 7 Build/JSS15Q) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/29.0.1547.72 Safari/537.36",
      touch: true,
    },
    {
      name: "Motorola Xoom, Xyboard",
      width: 1280,
      height: 800,
      pixelRatio: 1,
      userAgent: "Mozilla/5.0 (Linux; U; Android 3.0; en-us; Xoom Build/HRI39) AppleWebKit/525.10 (KHTML, like Gecko) Version/3.0.4 Mobile Safari/523.12.2",
      touch: true,
    },
    {
      name: "Samsung Galaxy Tab 7.7, 8.9, 10.1",
      width: 1280,
      height: 800,
      pixelRatio: 1,
      userAgent: "Mozilla/5.0 (Linux; U; Android 2.2; en-us; SCH-I800 Build/FROYO) AppleWebKit/533.1 (KHTML, like Gecko) Version/4.0 Mobile Safari/533.1",
      touch: true,
    },
    {
      name: "Samsung Galaxy Tab",
      width: 1024,
      height: 600,
      pixelRatio: 1,
      userAgent: "Mozilla/5.0 (Linux; U; Android 2.2; en-us; SCH-I800 Build/FROYO) AppleWebKit/533.1 (KHTML, like Gecko) Version/4.0 Mobile Safari/533.1",
      touch: true,
    },
  ],
  notebooks: [
    {
      name: "Notebook with touch",
      width: 1280,
      height: 950,
      pixelRatio: 1,
      userAgent: "",
      touch: true,
    },
    {
      name: "Notebook with HiDPI screen",
      width: 1440,
      height: 900,
      pixelRatio: 2,
      userAgent: "",
      touch: false,
    },
    {
      name: "Generic notebook",
      width: 1280,
      height: 800,
      pixelRatio: 1,
      userAgent: "",
      touch: false,
    },
  ],
  televisions: [
  ],
  watches: [
  ],
};
