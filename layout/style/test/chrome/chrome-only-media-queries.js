const CHROME_ONLY_TOGGLES = [
  "-moz-is-glyph",
  "-moz-print-preview",
  "-moz-non-native-content-theme",
  "-moz-scrollbar-start-backward",
  "-moz-scrollbar-start-forward",
  "-moz-scrollbar-end-backward",
  "-moz-scrollbar-end-forward",
  "-moz-scrollbar-thumb-proportional",
  "-moz-overlay-scrollbars",
  "-moz-windows-classic",
  "-moz-windows-compositor",
  "-moz-windows-default-theme",
  "-moz-mac-graphite-theme",
  "-moz-mac-big-sur-theme",
  "-moz-menubar-drag",
  "-moz-windows-accent-color-in-titlebar",
  "-moz-windows-compositor",
  "-moz-windows-classic",
  "-moz-windows-glass",
  "-moz-windows-non-native-menus",
  "-moz-swipe-animation-enabled",
  "-moz-gtk-csd-available",
  "-moz-gtk-csd-minimize-button",
  "-moz-gtk-csd-maximize-button",
  "-moz-gtk-csd-close-button",
  "-moz-gtk-csd-reversed-placement",
  "-moz-proton-places-tooltip",
];

// Non-parseable queries can be tested directly in
// `test_chrome_only_media_queries.html`.
const CHROME_ONLY_QUERIES = [
  "(-moz-os-version: windows-win7)",
  "(-moz-os-version: windows-win8)",
  "(-moz-os-version: windows-win10)",
];
