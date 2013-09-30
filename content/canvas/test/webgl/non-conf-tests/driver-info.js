DriverInfo = (function() {
  // ---------------------------------------------------------------------------
  // Debug info handling

  function defaultInfoFunc(str) {
    console.log('Info: ' + str);
  }

  var gInfoFunc = defaultInfoFunc;
  function setInfoFunc(func) {
    gInfoFunc = func;
  }

  function info(str) {
    gInfoFunc(str);
  }

  // ---------------------------------------------------------------------------
  // OS and driver identification
  //   Stolen from content/canvas/test/webgl/test_webgl_conformance_test_suite.html
  const Cc = SpecialPowers.Cc;
  const Ci = SpecialPowers.Ci;
  function detectDriverInfo() {
    var doc = Cc["@mozilla.org/xmlextras/domparser;1"].createInstance(Ci.nsIDOMParser).parseFromString("<html/>", "text/html");

    var canvas = doc.createElement("canvas");
    canvas.width = 1;
    canvas.height = 1;

    var type = "";
    var gl = null;
    try {
      gl = canvas.getContext("experimental-webgl");
    } catch(e) {}

    if (!gl) {
      info('Failed to create WebGL context for querying driver info.');
      throw 'WebGL failed';
    }

    var ext = gl.getExtension("WEBGL_debug_renderer_info");
    // this extension is unconditionally available to chrome. No need to check.

    var webglRenderer = gl.getParameter(ext.UNMASKED_RENDERER_WEBGL);
    var webglVendor = gl.getParameter(ext.UNMASKED_VENDOR_WEBGL);
    return [webglVendor, webglRenderer];
  }

  var OS = {
    WINDOWS: 'windows',
    MAC: 'mac',
    LINUX: 'linux',
    ANDROID: 'android',
    B2G: 'b2g',
  };

  var DRIVER = {
    MESA: 'mesa',
    NVIDIA: 'nvidia',
    ANDROID_X86_EMULATOR: 'android x86 emulator',
  };

  var kOS = null;
  var kOSVersion = null;
  var kDriver = null;

  // From reftest.js:
  var runtime = Cc['@mozilla.org/xre/app-info;1'].getService(Ci.nsIXULRuntime);

  if (navigator.platform.indexOf('Win') == 0) {
    kOS = OS.WINDOWS;

    // code borrowed from browser/modules/test/browser_taskbar_preview.js
    var version = SpecialPowers.Services.sysinfo.getProperty('version');
    kOSVersion = parseFloat(version);
    // Version 6.0 is Vista, 6.1 is 7.

  } else if (navigator.platform.indexOf('Mac') == 0) {
    kOS = OS.MAC;

    var versionMatch = /Mac OS X (\d+.\d+)/.exec(navigator.userAgent);
    kOSVersion = versionMatch ? parseFloat(versionMatch[1]) : null;

  } else if (runtime.widgetToolkit == 'gonk') {
    kOS = OS.B2G;

  } else if (navigator.appVersion.indexOf('Android') != -1) {
    kOS = OS.ANDROID;
    // From layout/tools/reftest/reftest.js:
    kOSVersion = SpecialPowers.Services.sysinfo.getProperty('version');

  } else if (navigator.platform.indexOf('Linux') == 0) {
    // Must be checked after android, as android also has a 'Linux' platform string.
    kOS = OS.LINUX;
  }

  try {
    var glVendor, glRenderer;
    [glVendor, glRenderer] = detectDriverInfo();
    info('GL vendor: ' + glVendor);
    info('GL renderer: ' + glRenderer);

    if (glRenderer.contains('llvmpipe')) {
      kDriver = DRIVER.MESA;
    } else if (glRenderer.contains('Android Emulator')) {
      kDriver = DRIVER.ANDROID_X86_EMULATOR;
    } else if (glVendor.contains('NVIDIA')) {
      kDriver = DRIVER.NVIDIA;
    }
  } catch (e) {
    // detectDriverInfo is fallible where WebGL fails.
  }

  if (kOS) {
    info('OS detected as: ' + kOS);
    info('  Version: ' + kOSVersion);
  } else {
    info('OS not detected.');
    info('  `platform`:   ' + navigator.platform);
    info('  `appVersion`: ' + navigator.appVersion);
    info('  `userAgent`:  ' + navigator.userAgent);
  }
  if (kDriver) {
    info('GL driver detected as: ' + kDriver);
  } else {
    info('GL driver not detected.');
  }

  return {
    setInfoFunc: setInfoFunc,

    OS: OS,
    DRIVER: DRIVER,
    getOS: function() { return kOS; },
    getDriver: function() { return kDriver; },
    getOSVersion: function() { return kOSVersion; },
  };
})();

