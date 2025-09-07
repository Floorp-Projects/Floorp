// SPDX-License-Identifier: MPL-2.0

type MozXULElement = {
  //https://searchfox.org/mozilla-central/rev/dbac9a2afcc5b1f112ed9b812d3daa7cbe71f951/toolkit/content/customElements.js#528
  /**
   * Allows eager deterministic construction of XUL elements with XBL attached, by
   * parsing an element tree and returning a DOM fragment to be inserted in the
   * document before any of the inner elements is referenced by JavaScript.
   *
   * This process is required instead of calling the createElement method directly
   * because bindings get attached when:
   *
   * 1. the node gets a layout frame constructed, or
   * 2. the node gets its JavaScript reflector created, if it's in the document,
   *
   * whichever happens first. The createElement method would return a JavaScript
   * reflector, but the element wouldn't be in the document, so the node wouldn't
   * get XBL attached. After that point, even if the node is inserted into a
   * document, it won't get XBL attached until either the frame is constructed or
   * the reflector is garbage collected and the element is touched again.
   *
   * @param str String with the XML representation of XUL elements.
   * @param [entities]  An array of DTD URLs containing entity definitions.
   * @return `DocumentFragment` instance containing
   *         the corresponding element tree, including element nodes
   *         but excluding any text node.
   */
  parseXULToFragment(str: string, entities?: Array<string>): DocumentFragment;
};
const nav_root = document.querySelector("#categories");
const fragment = (window.MozXULElement as MozXULElement).parseXULToFragment(`
    <richlistitem
      id="category-nora-link"
      class="category"
      align="center"
      tooltiptext="Nora Settings Link"
    >
      <image class="category-icon" />
      <label class="category-name" flex="1">
        Nora Settings Link
      </label>
    </richlistitem>
  `);
nav_root.appendChild(fragment);
document.querySelector("#category-nora-link").addEventListener("click", () => {
  if (import.meta.env.MODE === "dev") {
    window.location.href = "http://localhost:5183/";
  } else {
    const win = Services.wm.getMostRecentWindow("navigator:browser") as Window;

    win.gBrowser.selectedTab = win.gBrowser.addTab(
      "chrome://noraneko-settings/content/index.html",
      {
        relatedToCurrent: true,
        triggeringPrincipal:
          Services.scriptSecurityManager.getSystemPrincipal(),
      },
    );
  }
});
