{
  const { DOMLocalization } =
    ChromeUtils.import("resource://gre/modules/DOMLocalization.jsm", {});

  /**
   * Polyfill for document.ready polyfill.
   * See: https://github.com/whatwg/html/issues/127 for details.
   *
   * XXX: The callback is a temporary workaround for bug 1193394. Once Promises in Gecko
   *      start beeing a microtask and stop pushing translation post-layout, we can
   *      remove it and start using the returned Promise again.
   *
   * @param {Function} callback - function to be called when the document is ready.
   * @returns {Promise}
   */
  function documentReady(callback) {
    if (document.contentType === "application/vnd.mozilla.xul+xml") {
      // XUL
      return new Promise(
        resolve => document.addEventListener(
          "MozBeforeInitialXULLayout", () => {
            resolve(callback());
          }, { once: true }
        )
      );
    }

    // HTML
    const rs = document.readyState;
    if (rs === "interactive" || rs === "completed") {
      return Promise.resolve(callback);
    }
    return new Promise(
      resolve => document.addEventListener(
        "readystatechange", () => {
          resolve(callback());
        }, { once: true }
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

  document.l10n = new DOMLocalization(window, resourceIds);

  // trigger first context to be fetched eagerly
  document.l10n.ctxs.touchNext();

  document.l10n.ready = documentReady(() => {
    document.l10n.registerObservers();
    document.l10n.connectRoot(document.documentElement);
    return document.l10n.translateRoots();
  });
}
