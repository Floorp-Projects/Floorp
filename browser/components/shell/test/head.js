"use strict";

const { Subprocess } = ChromeUtils.import(
  "resource://gre/modules/Subprocess.jsm"
);

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

const screenshotPath = OS.Path.join(
  OS.Constants.Path.tmpDir,
  "headless_test_screenshot.png"
);

async function runFirefox(args) {
  const XRE_EXECUTABLE_FILE = "XREExeF";
  const firefoxExe = Services.dirsvc.get(XRE_EXECUTABLE_FILE, Ci.nsIFile).path;
  const NS_APP_PREFS_50_FILE = "PrefF";
  const mochiPrefsFile = Services.dirsvc.get(NS_APP_PREFS_50_FILE, Ci.nsIFile);
  const mochiPrefsPath = mochiPrefsFile.path;
  const mochiPrefsName = mochiPrefsFile.leafName;
  const profilePath = OS.Path.join(
    OS.Constants.Path.tmpDir,
    "headless_test_screenshot_profile"
  );
  const prefsPath = OS.Path.join(profilePath, mochiPrefsName);
  const firefoxArgs = ["-profile", profilePath, "-no-remote"];

  await OS.File.makeDir(profilePath);
  await OS.File.copy(mochiPrefsPath, prefsPath);
  let proc = await Subprocess.call({
    command: firefoxExe,
    arguments: firefoxArgs.concat(args),
    // Disable leak detection to avoid intermittent failure bug 1331152.
    environmentAppend: true,
    environment: {
      ASAN_OPTIONS:
        "detect_leaks=0:quarantine_size=50331648:malloc_context_size=5",
    },
  });
  let stdout;
  while ((stdout = await proc.stdout.readString())) {
    dump(`>>> ${stdout}\n`);
  }
  let { exitCode } = await proc.wait();
  is(exitCode, 0, "Firefox process should exit with code 0");
  await OS.File.removeDir(profilePath);
}

async function testFileCreationPositive(args, path) {
  await runFirefox(args);

  let saved = await OS.File.exists(path);
  ok(saved, "A screenshot should be saved as " + path);
  if (!saved) {
    return;
  }

  let info = await OS.File.stat(path);
  ok(info.size > 0, "Screenshot should not be an empty file");
  await OS.File.remove(path);
}

async function testFileCreationNegative(args, path) {
  await runFirefox(args);

  let saved = await OS.File.exists(path);
  ok(!saved, "A screenshot should not be saved");
  await OS.File.remove(path, { ignoreAbsent: true });
}

async function testWindowSizePositive(width, height) {
  let size = String(width);
  if (height) {
    size += "," + height;
  }

  await runFirefox([
    "-url",
    "http://mochi.test:8888/browser/browser/components/shell/test/headless.html",
    "-screenshot",
    screenshotPath,
    "-window-size",
    size,
  ]);

  let saved = await OS.File.exists(screenshotPath);
  ok(saved, "A screenshot should be saved in the tmp directory");
  if (!saved) {
    return;
  }

  let data = await OS.File.read(screenshotPath);
  await new Promise((resolve, reject) => {
    let blob = new Blob([data], { type: "image/png" });
    let reader = new FileReader();
    reader.onloadend = function() {
      let screenshot = new Image();
      screenshot.onloadend = function() {
        is(
          screenshot.width,
          width,
          "Screenshot should be " + width + " pixels wide"
        );
        if (height) {
          is(
            screenshot.height,
            height,
            "Screenshot should be " + height + " pixels tall"
          );
        }
        resolve();
      };
      screenshot.src = reader.result;
    };
    reader.readAsDataURL(blob);
  });
  await OS.File.remove(screenshotPath);
}

async function testGreen(url, path) {
  await runFirefox(["-url", url, `--screenshot=${path}`]);

  let saved = await OS.File.exists(path);
  ok(saved, "A screenshot should be saved in the tmp directory");
  if (!saved) {
    return;
  }

  let data = await OS.File.read(path);
  let image = await new Promise((resolve, reject) => {
    let blob = new Blob([data], { type: "image/png" });
    let reader = new FileReader();
    reader.onloadend = function() {
      let screenshot = new Image();
      screenshot.onloadend = function() {
        resolve(screenshot);
      };
      screenshot.src = reader.result;
    };
    reader.readAsDataURL(blob);
  });
  let canvas = document.createElement("canvas");
  canvas.width = image.naturalWidth;
  canvas.height = image.naturalHeight;
  let ctx = canvas.getContext("2d");
  ctx.drawImage(image, 0, 0);
  let imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
  let rgba = imageData.data;

  let found = false;
  for (let i = 0; i < rgba.length; i += 4) {
    if (rgba[i] === 0 && rgba[i + 1] === 255 && rgba[i + 2] === 0) {
      found = true;
      break;
    }
  }
  ok(found, "There should be a green pixel in the screenshot.");

  await OS.File.remove(path);
}
