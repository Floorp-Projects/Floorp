/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

window.addEventListener('DOMContentLoaded', function() {
  let viewport = new Viewport();
  let toolbar = new Toolbar(viewport);

  // Expose the custom viewport object to runtime
  window.createCustomViewport = function(actionHandler) {
    viewport.registerActionHandler(actionHandler);

    return {
      addView: viewport.addView.bind(viewport),
      clearView: viewport.clearView.bind(viewport),
      getBoundingClientRect: viewport.getBoundingClientRect.bind(viewport),
      is: viewport.is.bind(viewport),
      bindUIEvent: viewport.bindUIEvent.bind(viewport),
      unbindUIEvent: viewport.unbindUIEvent.bind(viewport),
      setCursor: viewport.setCursor.bind(viewport),
      getScrollOffset: viewport.getScrollOffset.bind(viewport),
      notify: viewport.notify.bind(viewport)
    };
  };
});
