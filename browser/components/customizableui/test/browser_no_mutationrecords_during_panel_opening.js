/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test that we don't get unexpected mutations during the opening of the
 * browser menu.
 */

add_task(function* test_setup() {
  yield resetCustomization();
  yield PanelUI.show();
  let hiddenPromise = promisePanelHidden(window);
  PanelUI.hide();
  yield hiddenPromise;
});

add_task(function* no_mutation_events_during_opening() {
  let panel = PanelUI.panel;
  yield PanelUI.ensureReady();

  let failures = 0;
  let observer = new MutationObserver(function(mutations) {
    for (let mutation of mutations) {
      if (mutation.target.localName == "panel" &&
          mutation.type == "attributes" &&
          mutation.attributeName == "animate") {
        // This mutation is allowed because it triggers the CSS transition.
        continue;
      }

      if (mutation.type == "attributes" &&
          mutation.attributeName == "panelopen") {
        // This mutation is allowed because it is set after the panel has
        // finished the transition.
        continue;
      }

      let newValue = null;
      if (mutation.type == "attributes") {
        newValue = mutation.target.getAttribute(mutation.attributeName);
      } else if (mutation.type == "characterData") {
        newValue = mutation.target.textContent;
      }

      if (AppConstants.isPlatformAndVersionAtMost("win", "6.1") &&
          mutation.target.className == "panel-arrowbox" &&
          mutation.attributeName == "style" &&
          newValue == "transform: translate(12px, 0px);") {
        // Windows 7 and before has a -12px alignment offset on the arrowbox.
        // This is allowed here as it is no longer used on newer platforms.
        continue;
      }

      if (newValue == mutation.oldValue) {
        // Mutations records are observed even when the new and old value are
        // identical. This is unlikely to invalidate the panel, so ignore these.
        continue;
      }

      let nodeIdentifier = `${mutation.target.localName}#${mutation.target.id}.${mutation.target.className};`;
      ok(false, `Observed: ${mutation.type}; ${nodeIdentifier} ${mutation.attributeName}; oldValue: ${mutation.oldValue}; newValue: ${newValue}`);
      failures++;
    }
  });
  observer.observe(panel, {
    childList: true,
    attributes: true,
    characterData: true,
    subtree: true,
    attributeOldValue: true,
    characterDataOldValue: true,
  });
  let shownPromise = promisePanelShown(window);
  PanelUI.show();
  yield shownPromise;
  observer.disconnect();

  is(failures, 0, "There should be no unexpected mutation events during opening of the panel");
});

add_task(function* cleanup() {
  let hiddenPromise = promisePanelHidden(window);
  PanelUI.hide();
  yield hiddenPromise;
});
