function waitForCertErrorLoad(browser) {
  return new Promise(resolve => {
    info("Waiting for DOMContentLoaded event");
    browser.addEventListener("DOMContentLoaded", function load() {
      browser.removeEventListener("DOMContentLoaded", load, false, true);
      resolve();
    }, false, true);
  });
}
