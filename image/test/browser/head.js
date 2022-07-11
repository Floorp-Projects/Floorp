const RELATIVE_DIR = "image/test/browser/";
const TESTROOT = "http://example.com/browser/" + RELATIVE_DIR;
const TESTROOT2 = "http://example.org/browser/" + RELATIVE_DIR;

var chrome_root = getRootDirectory(gTestPath);
const CHROMEROOT = chrome_root;

function getImageLoading(doc, id) {
  return doc.getElementById(id);
}

// Tries to get the Moz debug image, imgIContainerDebug. Only works
// in a debug build. If we succeed, we call func().
function actOnMozImage(doc, id, func) {
  var imgContainer = getImageLoading(doc, id).getRequest(
    Ci.nsIImageLoadingContent.CURRENT_REQUEST
  ).image;
  var mozImage;
  try {
    mozImage = imgContainer.QueryInterface(Ci.imgIContainerDebug);
  } catch (e) {
    return false;
  }
  func(mozImage);
  return true;
}

function assertPrefVal(name, val) {
  let boolValue = Services.prefs.getBoolPref(name);
  ok(boolValue === val, `pref ${name} is set to ${val}`);
  if (boolValue !== val) {
    throw Error(`pref ${name} is not set to ${val}`);
  }
}

function assertFileProcess() {
  // Ensure that the file content process is enabled.
  assertPrefVal("browser.tabs.remote.separateFileUriProcess", true);
}

function assertSandboxHeadless() {
  assertPrefVal("security.sandbox.content.headless", true);
}

function getPage() {
  let filePage = undefined;
  switch (Services.appinfo.OS) {
    case "WINNT":
      filePage = "file:///C:/";
      break;
    case "Darwin":
      filePage = "file:///tmp/";
      break;
    case "Linux":
      filePage = "file:///tmp/";
      break;
    default:
      throw new Error("Unsupported operating system");
  }
  return filePage;
}

function getSize() {
  let iconSize = undefined;
  switch (Services.appinfo.OS) {
    case "WINNT":
      iconSize = 32;
      break;
    case "Darwin":
      iconSize = 128;
      break;
    case "Linux":
      iconSize = 128;
      break;
    default:
      throw new Error("Unsupported operating system");
  }
  return iconSize;
}

async function createMozIconInFile(ext, expectSuccess = true) {
  const kPAGE = getPage();
  const kSize = expectSuccess ? getSize() : 24; // we get 24x24 when failing,
  // e.g. when remoting is
  // disabled and the sandbox
  // headless is enabled

  // open a tab in a file content process
  let fileTab = await BrowserTestUtils.addTab(gBrowser, kPAGE, {
    preferredRemoteType: "file",
  });

  // get the browser for the file content process tab
  let fileBrowser = gBrowser.getBrowserForTab(fileTab);

  let checkIcon = async (_ext, _kSize, _expectSuccess) => {
    const img = content.document.createElement("img");
    let waitLoad = new Promise(resolve => {
      // only listen to successfull load event if we expect the image to
      // actually load, e.g. with remoting disabled and sandbox headless
      // enabled we dont expect it to work, and we will wait for onerror below
      // to trigger.
      if (_expectSuccess) {
        img.addEventListener("load", resolve, { once: true });
      }
      img.onerror = () => {
        // With remoting enabled,
        // Verified to work by forcing early `return NS_ERROR_NOT_AVAILABLE;`
        // within `nsIconChannel::GetIcon(nsIURI* aURI, ByteBuf* aDataOut)`
        //
        // With remoting disabled and sandbox headless enabled, this should be
        // the default path, since we don't add the "load" event listener.
        ok(!_expectSuccess, "Error while loading moz-icon");
        resolve();
      };
    });
    img.setAttribute("src", `moz-icon://.${_ext}?size=${_kSize}`);
    img.setAttribute("id", `moz-icon-${_ext}-${_kSize}`);
    content.document.body.appendChild(img);

    await waitLoad;

    const icon = content.document.getElementById(`moz-icon-${_ext}-${_kSize}`);
    ok(icon !== null, `got a valid ${_ext} moz-icon`);
    is(icon.width, _kSize, `${_kSize} px width ${_ext} moz-icon`);
    is(icon.height, _kSize, `${_kSize} px height ${_ext} moz-icon`);
  };

  await BrowserTestUtils.browserLoaded(fileBrowser);
  await SpecialPowers.spawn(
    fileBrowser,
    [ext, kSize, expectSuccess],
    checkIcon
  );
  await BrowserTestUtils.removeTab(fileTab);
}
