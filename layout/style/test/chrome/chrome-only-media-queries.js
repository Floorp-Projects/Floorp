const CHROME_ONLY_TOGGLES = [
  "-moz-is-glyph",
  "-moz-print-preview",
  "-moz-scrollbar-start-backward",
  "-moz-scrollbar-start-forward",
  "-moz-scrollbar-end-backward",
  "-moz-scrollbar-end-forward",
  "-moz-overlay-scrollbars",
  "-moz-mac-big-sur-theme",
  "-moz-menubar-drag",
  "-moz-windows-accent-color-in-titlebar",
  "-moz-swipe-animation-enabled",
  "-moz-gtk-csd-available",
  "-moz-gtk-csd-minimize-button",
  "-moz-gtk-csd-maximize-button",
  "-moz-gtk-csd-close-button",
  "-moz-gtk-csd-reversed-placement",
  "-moz-panel-animations",
];

// Non-parseable queries can be tested directly in
// `test_chrome_only_media_queries.html`.
const CHROME_ONLY_QUERIES = [
  "(-moz-platform: linux)",
  "(-moz-platform: windows)",
  "(-moz-platform: macos)",
  "(-moz-platform: android)",
  "(-moz-content-prefers-color-scheme: dark)",
  "(-moz-content-prefers-color-scheme: light)",
  "(-moz-gtk-theme-family: unknown)",
  "(-moz-gtk-theme-family: adwaita)",
  "(-moz-gtk-theme-family: breeze)",
  "(-moz-gtk-theme-family: yaru)",
];
