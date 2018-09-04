{
  const { DOMLocalization } =
    ChromeUtils.import("resource://gre/modules/DOMLocalization.jsm", {});
  const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm", {});

  /**
   * Polyfill for document.ready polyfill.
   * See: https://github.com/whatwg/html/issues/127 for details.
   *
   * @returns {Promise}
   */
  function documentReady() {
    if (document.contentType === "application/vnd.mozilla.xul+xml") {
      // XUL
      return new Promise(
        resolve => document.addEventListener(
          "MozBeforeInitialXULLayout", resolve, { once: true }
        )
      );
    }

    // HTML
    const rs = document.readyState;
    if (rs === "interactive" || rs === "completed") {
      return Promise.resolve();
    }
    return new Promise(
      resolve => document.addEventListener(
        "readystatechange", resolve, { once: true }
      )
    );
  }

  /**
   * Scans the `elem` for links with localization resources.
   *
   * @param {Element} elem
   * @returns {Array<string>}
   */
  function getResourceLinks(elem) {
    return Array.from(elem.querySelectorAll('link[rel="localization"]')).map(
      el => el.getAttribute("href")
    );
  }

  const resourceIds = getResourceLinks(document.head || document);

  document.l10n = new DOMLocalization(resourceIds);

  const appLocales = Services.locale.getAppLocalesAsBCP47();
  const prefetchCount = appLocales.length > 1 ? 2 : 1;
  document.l10n.ctxs.touchNext(prefetchCount);

  document.l10n.ready = documentReady().then(() => {
    document.l10n.registerObservers();
    document.l10n.connectRoot(document.documentElement);
    return document.l10n.translateRoots();
  });
}
