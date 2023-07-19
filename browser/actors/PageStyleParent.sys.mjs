/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class PageStyleParent extends JSWindowActorParent {
  // This has the most recent information about the content stylesheets for
  // that actor. It's populated via the PageStyle:Add and PageStyle:Clear
  // messages from the content process. It has the following structure:
  //
  // filteredStyleSheets (Array):
  //   An Array of objects with a filtered list representing all stylesheets
  //   that the current page offers. Each object has the following members:
  //
  //   title (String):
  //     The title of the stylesheet
  //
  //   disabled (bool):
  //     Whether or not the stylesheet is currently applied
  //
  //   href (String):
  //     The URL of the stylesheet. Stylesheets loaded via a data URL will
  //     have this property set to null.
  //
  // preferredStyleSheetSet (bool):
  //   Whether or not the user currently has the "Default" style selected
  //   for the current page.
  #styleSheetInfo = null;

  receiveMessage(msg) {
    // Check if things are alive:
    let browser = this.browsingContext.top.embedderElement;
    if (!browser || browser.ownerGlobal.closed) {
      return;
    }

    // We always store information at the top of the frame tree.
    let actor =
      this.browsingContext.top.currentWindowGlobal.getActor("PageStyle");
    switch (msg.name) {
      case "PageStyle:Add":
        actor.addSheetInfo(msg.data);
        break;
      case "PageStyle:Clear":
        if (actor == this) {
          this.#styleSheetInfo = null;
        }
        break;
    }
  }

  /**
   * Add/append styleSheets to the _pageStyleSheets weakmap.
   * @param newSheetData
   *        The stylesheet data, including new stylesheets to add,
   *        and the preferred stylesheet set for this document.
   */
  addSheetInfo(newSheetData) {
    let info = this.getSheetInfo();
    info.filteredStyleSheets.push(...newSheetData.filteredStyleSheets);
    info.preferredStyleSheetSet ||= newSheetData.preferredStyleSheetSet;
  }

  getSheetInfo() {
    if (!this.#styleSheetInfo) {
      this.#styleSheetInfo = {
        filteredStyleSheets: [],
        preferredStyleSheetSet: true,
      };
    }
    return this.#styleSheetInfo;
  }
}
