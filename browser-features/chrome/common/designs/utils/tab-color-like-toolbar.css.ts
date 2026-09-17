/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/** Nav/toolbar surface color. Prefer the legacy token selected tabs still track. */
export const TOOLBAR_SURFACE =
  "var(--toolbar-bgcolor, var(--toolbar-background-color))";

/**
 * Fluerial-only: selected tab layout + rounded bottom corners.
 * Restores --tab-block-margin top gap (TAB_COLOR_LIKE_TOOLBAR fills .tab-content).
 */
export const FLUERIAL_TAB_CORNER_CSS = `
#TabsToolbar
  #tabbrowser-tabs
  .tabbrowser-tab:is([visuallyselected], [multiselected])
  > .tab-stack
  > .tab-content {
  margin-block-start: var(--tab-block-margin, 4px) !important;
  margin-block-end: 0 !important;
  border-radius: 8px 8px 0 0 !important;
  height: calc(100% - var(--tab-block-margin, 4px)) !important;
}

#TabsToolbar
  #tabbrowser-tabs
  .tabbrowser-tab:is([visuallyselected], [multiselected])
  > .tab-stack
  > .tab-background:is([selected], [multiselected]) {
  margin-block-start: var(--tab-block-margin, 4px) !important;
  margin-block-end: 0 !important;
}

#TabsToolbar
  #tabbrowser-tabs
  .tabbrowser-tab:is([visuallyselected], [multiselected])
  > .tab-stack
  > .tab-background:is([selected], [multiselected])::before {
  box-shadow: 4px 4px 0 4px ${TOOLBAR_SURFACE} !important;
}

#TabsToolbar
  #tabbrowser-tabs
  .tabbrowser-tab:is([visuallyselected], [multiselected])
  > .tab-stack
  > .tab-background:is([selected], [multiselected])::after {
  box-shadow: -4px 4px 0 4px ${TOOLBAR_SURFACE} !important;
}
`;

/**
 * Make selected tabs use the nav-bar / toolbar surface color.
 * Gecko 152 paints tab fill via --tab-background-color-selected and .tab-content.
 * Does not modify #nav-bar.
 */
export const TAB_COLOR_LIKE_TOOLBAR_CSS = `
:root {
  --tab-background-color-selected: ${TOOLBAR_SURFACE} !important;
}

:root:is(:not([lwtheme]), :not(:-moz-lwtheme)) {
  --tab-selected-bgcolor: unset !important;
  --tab-selected-bgimage: unset !important;
}

#TabsToolbar
  .tabbrowser-tab:is([visuallyselected], [multiselected])
  > .tab-stack
  > .tab-content {
  background-color: ${TOOLBAR_SURFACE} !important;
}

#TabsToolbar #firefox-view-button[open] > .toolbarbutton-icon,
#TabsToolbar
  .tabbrowser-tab
  > .tab-stack
  > .tab-background:is([selected], [multiselected]) {
  background-color: ${TOOLBAR_SURFACE} !important;
}

:root:is(:-moz-lwtheme, [lwtheme])
  #TabsToolbar
  #tabbrowser-tabs:not([movingtab])
  .tabbrowser-tab:is([visuallyselected], [multiselected])
  > .tab-stack
  > .tab-background:is([selected], [multiselected]) {
  background-image:
    linear-gradient(transparent, transparent),
    linear-gradient(${TOOLBAR_SURFACE}, ${TOOLBAR_SURFACE}),
    var(--lwt-header-image, none) !important;
  background-position: 0, 0, right top;
  background-attachment: scroll, scroll, fixed;
  background-repeat: repeat-x, repeat-x, no-repeat !important;
}
`;
