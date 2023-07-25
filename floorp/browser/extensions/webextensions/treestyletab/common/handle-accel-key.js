/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

(function() {
  if (window.handleAccelKeyLoaded)
    return;

  function stringifySoloModifier(event) {
    switch (event.key) {
      case 'Alt':
        return (
          !event.ctrlKey &&
          !event.metaKey
        ) ? 'alt' : null;

      case 'Control':
        return (
          !event.altKey &&
          !event.metaKey
        ) ? 'control' : null;

      case 'Meta':
        return (
          !event.altKey &&
          !event.ctrlKey
        ) ? 'meta' : null;

      default:
        return null;
    }
  }

  function stringifyModifier(event) {
    if (event.altKey &&
        !event.ctrlKey &&
        !event.metaKey)
      return 'alt';

    if (!event.altKey &&
        event.ctrlKey &&
        !event.metaKey)
      return 'control';

    if (!event.altKey &&
        !event.ctrlKey &&
        event.metaKey)
      return 'meta';

    return null;
  }

  function stringifyUnshiftedSoloModifier(event) {
    if (event.key != 'Shift')
      return null;
    return stringifyModifier(event);
  }

  function stringifyMayTabSwitchModifier(event) {
    if (!/^(Tab|Shift|PageUp|PageDown)$/.test(event.key))
      return null;
    return stringifyModifier(event);
  }

  function onKeyDown(event) {
    const modifier             = stringifySoloModifier(event);
    const mayTabSwitchModifier = stringifyMayTabSwitchModifier(event);
    if (modifier ||
        mayTabSwitchModifier)
      browser.runtime.sendMessage({
        type:     'treestyletab:notify-may-start-tab-switch',
        modifier: modifier || mayTabSwitchModifier
      });
  }

  function onKeyUp(event) {
    const modifier             = stringifySoloModifier(event);
    const unshiftedModifier    = stringifyUnshiftedSoloModifier(event);
    const mayTabSwitchModifier = stringifyMayTabSwitchModifier(event);
    if (modifier ||
        (!unshiftedModifier &&
         !mayTabSwitchModifier))
      browser.runtime.sendMessage({
        type:     'treestyletab:notify-may-end-tab-switch',
        modifier: modifier || stringifyModifier(event)
      });
  }

  function init() {
    window.handleAccelKeyLoaded = true;
    window.addEventListener('keydown', onKeyDown, { capture: true });
    window.addEventListener('keyup', onKeyUp, { capture: true });
    window.addEventListener('pagehide', () => {
      window.addEventListener('keydown', onKeyDown, { capture: true });
      window.addEventListener('keyup', onKeyUp, { capture: true });
      window.addEventListener('pageshow', init, { once: true });
    }, { once: true });
  }
  init();
})();
