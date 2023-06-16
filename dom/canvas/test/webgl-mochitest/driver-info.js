DriverInfo = (function () {
  // ---------------------------------------------------------------------------
  // Debug info handling

  function info(str) {
    window.console.log("Info: " + str);
  }

  // ---------------------------------------------------------------------------
  // OS and driver identification
  //   Stolen from dom/canvas/test/webgl/test_webgl_conformance_test_suite.html
  function detectDriverInfo() {
    var canvas = document.createElement("canvas");
    canvas.width = 1;
    canvas.height = 1;

    var type = "";
    var gl = null;
    try {
      gl = canvas.getContext("experimental-webgl");
    } catch (e) {}

    if (!gl) {
      info("Failed to create WebGL context for querying driver info.");
      throw "WebGL failed";
    }

    var ext = gl.getExtension("WEBGL_debug_renderer_info");
    if (!ext) {
      throw "WEBGL_debug_renderer_info not available";
    }

    var webglRenderer = gl.getParameter(ext.UNMASKED_RENDERER_WEBGL);
    return webglRenderer;
  }

  function detectOSInfo() {
    var os = null;
    var version = null;
    if (navigator.platform.indexOf("Win") == 0) {
      os = OS.WINDOWS;

      var versionMatch = /Windows NT (\d+.\d+)/.exec(navigator.userAgent);
      version = versionMatch ? parseFloat(versionMatch[1]) : null;
      // Version 6.0 is Vista, 6.1 is 7.
    } else if (navigator.platform.indexOf("Mac") == 0) {
      os = OS.MAC;

      var versionMatch = /Mac OS X (\d+.\d+)/.exec(navigator.userAgent);
      version = versionMatch ? parseFloat(versionMatch[1]) : null;
    } else if (navigator.appVersion.includes("Android")) {
      os = OS.ANDROID;

      try {
        // From layout/tools/reftest/reftest.js:
        version = SpecialPowers.Services.sysinfo.getProperty("version");
      } catch (e) {
        info("No SpecialPowers: can't query android version");
      }
    } else if (navigator.platform.indexOf("Linux") == 0) {
      // Must be checked after android, as android also has a 'Linux' platform string.
      os = OS.LINUX;
    }

    return [os, version];
  }

  var OS = {
    WINDOWS: "windows",
    MAC: "mac",
    LINUX: "linux",
    ANDROID: "android",
  };

  var DRIVER = {
    INTEL: "intel",
    MESA: "mesa",
    NVIDIA: "nvidia",
    ANDROID_X86_EMULATOR: "android x86 emulator",
    ANGLE: "angle",
  };

  var kOS = null;
  var kOSVersion = null;
  var kRawDriver = null;
  var kDriver = null;

  try {
    [kOS, kOSVersion] = detectOSInfo();
  } catch (e) {
    // Generally just fails when we don't have SpecialPowers.
  }

  try {
    kRawDriver = detectDriverInfo();

    if (kRawDriver.includes("llvmpipe")) {
      kDriver = DRIVER.MESA;
    } else if (kRawDriver.includes("Android Emulator")) {
      kDriver = DRIVER.ANDROID_X86_EMULATOR;
    } else if (kRawDriver.includes("ANGLE")) {
      kDriver = DRIVER.ANGLE;
    } else if (kRawDriver.includes("NVIDIA")) {
      kDriver = DRIVER.NVIDIA;
    } else if (kRawDriver.includes("Intel")) {
      kDriver = DRIVER.INTEL;
    }
  } catch (e) {
    // detectDriverInfo is fallible where WebGL fails.
  }

  function dump(line_out_func) {
    let lines = [
      "[DriverInfo] userAgent: " + navigator.userAgent,
      "[DriverInfo] kRawDriver: " + kRawDriver,
      "[DriverInfo] kDriver: " + kDriver,
      "[DriverInfo] kOS: " + kOS,
      "[DriverInfo] kOSVersion: " + kOSVersion,
    ];
    lines.forEach(line_out_func);
  }

  dump(x => console.log(x));

  return {
    DRIVER,
    OS,
    dump,
    getDriver() {
      return kDriver;
    },
    getOS() {
      return kOS;
    },
    getOSVersion() {
      return kOSVersion;
    },
  };
})();
