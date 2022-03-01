/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
/*
 original:
   http://github.com/piroor/webextensions-lib-tab-favicon-helper
*/
'use strict';

const TabFavIconHelper = {
  LAST_EFFECTIVE_FAVICON: 'last-effective-favIcon',
  VALID_FAVICON_PATTERN: /^(about|app|chrome|data|file|ftp|https?|moz-extension|resource):/,
  DRAWABLE_FAVICON_PATTERN: /^(https?|moz-extension|resource):/,

  // original: chrome://browser/content/aboutlogins/icons/favicon.svg
  FAVICON_LOCKWISE: `
<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="32" height="32">
  <defs>
    <linearGradient id="b" x1="24.684" y1="5.756" x2="6.966" y2="26.663" gradientUnits="userSpaceOnUse">
      <stop offset="0" stop-color="#ff9640"/>
      <stop offset=".6" stop-color="#fc4055"/>
      <stop offset="1" stop-color="#e31587"/>
    </linearGradient>
    <linearGradient id="c" x1="26.362" y1="4.459" x2="10.705" y2="21.897" gradientUnits="userSpaceOnUse">
      <stop offset="0" stop-color="#fff36e" stop-opacity=".8"/>
      <stop offset=".094" stop-color="#fff36e" stop-opacity=".699"/>
      <stop offset=".752" stop-color="#fff36e" stop-opacity="0"/>
    </linearGradient>
    <linearGradient id="d" x1="7.175" y1="27.275" x2="23.418" y2="11.454" gradientUnits="userSpaceOnUse">
      <stop offset="0" stop-color="#0090ed"/>
      <stop offset=".386" stop-color="#5b6df8"/>
      <stop offset=".629" stop-color="#9059ff"/>
      <stop offset="1" stop-color="#b833e1"/>
    </linearGradient>
    <linearGradient id="a" x1="29.104" y1="15.901" x2="26.135" y2="21.045" gradientUnits="userSpaceOnUse">
      <stop offset="0" stop-color="#722291" stop-opacity=".5"/>
      <stop offset=".5" stop-color="#722291" stop-opacity="0"/>
    </linearGradient>
    <linearGradient id="e" x1="20.659" y1="21.192" x2="13.347" y2="28.399" xlink:href="#a"/>
    <linearGradient id="f" x1="2.97" y1="19.36" x2="10.224" y2="21.105" gradientUnits="userSpaceOnUse">
      <stop offset="0" stop-color="#054096" stop-opacity=".5"/>
      <stop offset=".054" stop-color="#0f3d9c" stop-opacity=".441"/>
      <stop offset=".261" stop-color="#2f35b1" stop-opacity=".249"/>
      <stop offset=".466" stop-color="#462fbf" stop-opacity=".111"/>
      <stop offset=".669" stop-color="#542bc8" stop-opacity=".028"/>
      <stop offset=".864" stop-color="#592acb" stop-opacity="0"/>
    </linearGradient>
  </defs>
  <path d="M15.986 31.076A5.635 5.635 0 0 1 12.3 29.76 103.855 103.855 0 0 1 2.249 19.7a5.841 5.841 0 0 1-.006-7.4A103.792 103.792 0 0 1 12.3 2.247a5.837 5.837 0 0 1 7.4-.006A104.1 104.1 0 0 1 29.751 12.3a5.842 5.842 0 0 1 .006 7.405c-.423.484-.584.661-.917 1.025l-.234.255a1.749 1.749 0 1 1-2.585-2.357l.234-.258c.314-.344.467-.511.86-.961a2.352 2.352 0 0 0-.008-2.814 100.308 100.308 0 0 0-9.7-9.714 2.352 2.352 0 0 0-2.814.007 100.323 100.323 0 0 0-9.7 9.7 2.354 2.354 0 0 0 .006 2.815 100.375 100.375 0 0 0 9.7 9.708 2.352 2.352 0 0 0 2.815-.006c1.311-1.145 2.326-2.031 3.434-3.086l-3.4-3.609a2.834 2.834 0 0 1-.776-2.008 2.675 2.675 0 0 1 .834-1.9l.194-.184a2.493 2.493 0 0 0 1.124-2.333 2.81 2.81 0 0 0-5.6 0 2.559 2.559 0 0 0 1.092 2.279l.127.118a2.735 2.735 0 0 1 .324 3.7 1.846 1.846 0 0 1-.253.262l-1.578 1.326a1.75 1.75 0 0 1-2.252-2.68l.755-.634a5.758 5.758 0 0 1-1.715-4.366 6.305 6.305 0 0 1 12.6 0 5.642 5.642 0 0 1-1.854 4.528l3.84 4.082a2.071 2.071 0 0 1 .59 1.446 2.128 2.128 0 0 1-.628 1.526c-1.6 1.592-2.895 2.72-4.533 4.15a5.745 5.745 0 0 1-3.753 1.354z" fill="url(#b)"/>
  <path d="M15.986 31.076A5.635 5.635 0 0 1 12.3 29.76 103.855 103.855 0 0 1 2.249 19.7a5.841 5.841 0 0 1-.006-7.4A103.792 103.792 0 0 1 12.3 2.247a5.837 5.837 0 0 1 7.4-.006A104.1 104.1 0 0 1 29.751 12.3a5.842 5.842 0 0 1 .006 7.405c-.423.484-.584.661-.917 1.025l-.234.255a1.749 1.749 0 1 1-2.585-2.357l.234-.258c.314-.344.467-.511.86-.961a2.352 2.352 0 0 0-.008-2.814 100.308 100.308 0 0 0-9.7-9.714 2.352 2.352 0 0 0-2.814.007 100.323 100.323 0 0 0-9.7 9.7 2.354 2.354 0 0 0 .006 2.815 100.375 100.375 0 0 0 9.7 9.708 2.352 2.352 0 0 0 2.815-.006c1.311-1.145 2.326-2.031 3.434-3.086l-3.4-3.609a2.834 2.834 0 0 1-.776-2.008 2.675 2.675 0 0 1 .834-1.9l.194-.184a2.493 2.493 0 0 0 1.124-2.333 2.81 2.81 0 0 0-5.6 0 2.559 2.559 0 0 0 1.092 2.279l.127.118a2.735 2.735 0 0 1 .324 3.7 1.846 1.846 0 0 1-.253.262l-1.578 1.326a1.75 1.75 0 0 1-2.252-2.68l.755-.634a5.758 5.758 0 0 1-1.715-4.366 6.305 6.305 0 0 1 12.6 0 5.642 5.642 0 0 1-1.854 4.528l3.84 4.082a2.071 2.071 0 0 1 .59 1.446 2.128 2.128 0 0 1-.628 1.526c-1.6 1.592-2.895 2.72-4.533 4.15a5.745 5.745 0 0 1-3.753 1.354z" fill="url(#c)"/>
  <path d="M29.58 17.75a34.267 34.267 0 0 0-2.473-3.15 2.352 2.352 0 0 1 .008 2.814c-.393.45-.546.617-.86.961l-.234.258a1.749 1.749 0 1 0 2.585 2.357l.234-.255c.231-.253.379-.415.59-.653a2.161 2.161 0 0 0 .15-2.332zm-8.734 6.275c-1.108 1.055-2.123 1.941-3.434 3.086a2.352 2.352 0 0 1-2.815.006 100.375 100.375 0 0 1-9.7-9.708 2.354 2.354 0 0 1-.006-2.815s-2.131 1.984-2.4 3.424a2.956 2.956 0 0 0 .724 2.782 103.772 103.772 0 0 0 9.085 8.96 5.635 5.635 0 0 0 3.683 1.316 5.745 5.745 0 0 0 3.753-1.351c.926-.808 1.741-1.52 2.565-2.273a1.558 1.558 0 0 0 0-1.476 11.55 11.55 0 0 0-1.455-1.951z" fill="url(#d)"/>
  <path d="M29.43 20.079c-.211.238-.359.4-.59.653l-.234.255a1.749 1.749 0 1 1-2.585-2.357l.234-.258c.314-.344.467-.511.86-.961a2.352 2.352 0 0 0-.008-2.814 34.267 34.267 0 0 1 2.473 3.153 2.161 2.161 0 0 1-.15 2.329z" fill="url(#a)"/>
  <path d="M22.3 25.976a11.55 11.55 0 0 0-1.458-1.951c-1.108 1.055-2.123 1.941-3.434 3.086a2.352 2.352 0 0 1-2.815.006c-1.414-1.234-2.768-2.5-4.095-3.8L8.023 25.8c1.384 1.35 2.8 2.668 4.28 3.958a5.635 5.635 0 0 0 3.683 1.316 5.745 5.745 0 0 0 3.753-1.351c.926-.808 1.741-1.52 2.565-2.273a1.558 1.558 0 0 0-.004-1.474z" fill="url(#e)"/>
  <path d="M4.892 17.409a2.354 2.354 0 0 1-.006-2.815s-2.131 1.984-2.4 3.424a2.956 2.956 0 0 0 .729 2.782c1.56 1.739 3.158 3.4 4.808 5.007l2.479-2.48c-1.935-1.891-3.802-3.844-5.61-5.918z" opacity=".9" fill="url(#f)"/>
</svg>
`,
  // original: chrome://browser/content/robot.ico
  FAVICON_ROBOT: `
data:image/x-icon;base64,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
`.trim(),
  // original: chrome://browser/skin/controlcenter/dashboard.svg
  FAVICON_DASHBOARD: `
<svg data-name="icon" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16">
  <path fill="context-fill" fill-opacity="context-fill-opacity" d="M15 12H4a2 2 0 0 1-2-2V3a1 1 0 0 0-2 0v7a4 4 0 0 0 4 4h11a1 1 0 0 0 0-2z"/>
  <path fill="context-fill" fill-opacity="context-fill-opacity" d="M4 11a1 1 0 0 0 1-1V8a1 1 0 0 0-2 0v2a1 1 0 0 0 1 1zM7 11a1 1 0 0 0 1-1V4a1 1 0 0 0-2 0v6a1 1 0 0 0 1 1zM10 11a1 1 0 0 0 1-1V6a1 1 0 0 0-2 0v4a1 1 0 0 0 1 1zM13 11a1 1 0 0 0 1-1V7a1 1 0 0 0-2 0v3a1 1 0 0 0 1 1z"/>
</svg>
`,
  // original: chrome://browser/skin/developer.svg
  FAVICON_DEVELOPER: `
<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
  <path fill="context-fill" fill-opacity="context-fill-opacity" d="M14.555 3.2l-2.434 2.436a1.243 1.243 0 1 1-1.757-1.757L12.8 1.445A3.956 3.956 0 0 0 11 1a3.976 3.976 0 0 0-3.434 6.02l-6.273 6.273a1 1 0 1 0 1.414 1.414L8.98 8.434A3.96 3.96 0 0 0 11 9a4 4 0 0 0 4-4 3.956 3.956 0 0 0-.445-1.8z"/>
</svg>
`,
  // original: chrome://browser/skin/privatebrowsing/favicon.svg
  FAVICON_PRIVATE_BROWSING: `
<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
  <circle cx="8" cy="8" r="8" fill="#8d20ae"/>
  <circle cx="8" cy="8" r="7.5" stroke="#7b149a" stroke-width="1" fill="none"/>
  <path d="M11.309,10.995C10.061,10.995,9.2,9.5,8,9.5s-2.135,1.5-3.309,1.5c-1.541,0-2.678-1.455-2.7-3.948C1.983,5.5,2.446,5.005,4.446,5.005S7.031,5.822,8,5.822s1.555-.817,3.555-0.817S14.017,5.5,14.006,7.047C13.987,9.54,12.85,10.995,11.309,10.995ZM5.426,6.911a1.739,1.739,0,0,0-1.716.953A2.049,2.049,0,0,0,5.3,8.544c0.788,0,1.716-.288,1.716-0.544A1.428,1.428,0,0,0,5.426,6.911Zm5.148,0A1.429,1.429,0,0,0,8.981,8c0,0.257.928,0.544,1.716,0.544a2.049,2.049,0,0,0,1.593-.681A1.739,1.739,0,0,0,10.574,6.911Z" stroke="#670c83" stroke-width="2" fill="none"/>
  <path d="M11.309,10.995C10.061,10.995,9.2,9.5,8,9.5s-2.135,1.5-3.309,1.5c-1.541,0-2.678-1.455-2.7-3.948C1.983,5.5,2.446,5.005,4.446,5.005S7.031,5.822,8,5.822s1.555-.817,3.555-0.817S14.017,5.5,14.006,7.047C13.987,9.54,12.85,10.995,11.309,10.995ZM5.426,6.911a1.739,1.739,0,0,0-1.716.953A2.049,2.049,0,0,0,5.3,8.544c0.788,0,1.716-.288,1.716-0.544A1.428,1.428,0,0,0,5.426,6.911Zm5.148,0A1.429,1.429,0,0,0,8.981,8c0,0.257.928,0.544,1.716,0.544a2.049,2.049,0,0,0,1.593-.681A1.739,1.739,0,0,0,10.574,6.911Z" fill="#fff"/>
</svg>
`,
  // original: chrome://browser/skin/settings.svg
  FAVICON_SETTINGS: `
<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
  <path fill="context-fill" fill-opacity="context-fill-opacity" d="M15 7h-2.1a4.967 4.967 0 0 0-.732-1.753l1.49-1.49a1 1 0 0 0-1.414-1.414l-1.49 1.49A4.968 4.968 0 0 0 9 3.1V1a1 1 0 0 0-2 0v2.1a4.968 4.968 0 0 0-1.753.732l-1.49-1.49a1 1 0 0 0-1.414 1.415l1.49 1.49A4.967 4.967 0 0 0 3.1 7H1a1 1 0 0 0 0 2h2.1a4.968 4.968 0 0 0 .737 1.763c-.014.013-.032.017-.045.03l-1.45 1.45a1 1 0 1 0 1.414 1.414l1.45-1.45c.013-.013.018-.031.03-.045A4.968 4.968 0 0 0 7 12.9V15a1 1 0 0 0 2 0v-2.1a4.968 4.968 0 0 0 1.753-.732l1.49 1.49a1 1 0 0 0 1.414-1.414l-1.49-1.49A4.967 4.967 0 0 0 12.9 9H15a1 1 0 0 0 0-2zM5 8a3 3 0 1 1 3 3 3 3 0 0 1-3-3z"/>
</svg>
`,
  // original: chrome://browser/skin/window.svg
  FAVICON_WINDOW: `
<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
  <path fill="context-fill" fill-opacity="context-fill-opacity" d="M13 1H3a3 3 0 0 0-3 3v8a3 3 0 0 0 3 3h11a2 2 0 0 0 2-2V4a3 3 0 0 0-3-3zm1 11a1 1 0 0 1-1 1H3a1 1 0 0 1-1-1V6h12zm0-7H2V4a1 1 0 0 1 1-1h10a1 1 0 0 1 1 1z"/>
</svg>
`,
  // original: chrome://devtools/skin/images/profiler-stopwatch.svg
  FAVICON_PROFILER: `
<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
  <path fill="context-fill" fill-opacity="context-fill-opacity" d="M10.85 6.85a.5.5 0 0 0-.7-.7l-2.5 2.5.7.7 2.5-2.5zM8 10a1 1 0 1 0 0-2 1 1 0 0 0 0 2zM5 1a1 1 0 0 1 1-1h4a1 1 0 1 1 0 2H6a1 1 0 0 1-1-1zM8 4a5 5 0 1 0 0 10A5 5 0 0 0 8 4zM1 9a7 7 0 1 1 14 0A7 7 0 0 1 1 9z"/>
</svg>
`,
  // original: chrome://global/skin/icons/performance.svg
  FAVICON_PERFORMANCE: `
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16">
  <path fill="context-fill" d="M8 1a8 8 0 0 0-8 8 7.89 7.89 0 0 0 .78 3.43 1 1 0 0 0 .9.57.94.94 0 0 0 .43-.1 1 1 0 0 0 .47-1.33A6.07 6.07 0 0 1 2 9a6 6 0 0 1 12 0 5.93 5.93 0 0 1-.59 2.57 1 1 0 0 0 1.81.86A7.89 7.89 0 0 0 16 9a8 8 0 0 0-8-8z"/>
  <path fill="context-fill" d="M11.77 7.08a.5.5 0 0 0-.69.15L8.62 11.1A2.12 2.12 0 0 0 8 11a2 2 0 0 0 0 4 2.05 2.05 0 0 0 1.12-.34 2.31 2.31 0 0 0 .54-.54 2 2 0 0 0 0-2.24 2.31 2.31 0 0 0-.2-.24l2.46-3.87a.5.5 0 0 0-.15-.69z"/>
</svg>
`,
  // original: chrome://global/skin/icons/warning.svg
  FAVICON_WARNING: `
<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
  <path fill="context-fill" fill-opacity="context-fill-opacity" d="M14.742 12.106L9.789 2.2a2 2 0 0 0-3.578 0l-4.953 9.91A2 2 0 0 0 3.047 15h9.905a2 2 0 0 0 1.79-2.894zM7 5a1 1 0 0 1 2 0v4a1 1 0 0 1-2 0zm1 8.25A1.25 1.25 0 1 1 9.25 12 1.25 1.25 0 0 1 8 13.25z"/>
</svg>
`,
  // original: chrome://mozapps/skin/extensions/extensionGeneric-16.svg
  FAVICON_EXTENSION: `
<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="context-fill" fill-opacity="context-fill-opacity">
  <path d="M14.5 8c-.971 0-1 1-1.75 1a.765.765 0 0 1-.75-.75V5a1 1 0 0 0-1-1H7.75A.765.765 0 0 1 7 3.25c0-.75 1-.779 1-1.75C8 .635 7.1 0 6 0S4 .635 4 1.5c0 .971 1 1 1 1.75a.765.765 0 0 1-.75.75H1a1 1 0 0 0-1 1v2.25A.765.765 0 0 0 .75 8c.75 0 .779-1 1.75-1C3.365 7 4 7.9 4 9s-.635 2-1.5 2c-.971 0-1-1-1.75-1a.765.765 0 0 0-.75.75V15a1 1 0 0 0 1 1h3.25a.765.765 0 0 0 .75-.75c0-.75-1-.779-1-1.75 0-.865.9-1.5 2-1.5s2 .635 2 1.5c0 .971-1 1-1 1.75a.765.765 0 0 0 .75.75H11a1 1 0 0 0 1-1v-3.25a.765.765 0 0 1 .75-.75c.75 0 .779 1 1.75 1 .865 0 1.5-.9 1.5-2s-.635-2-1.5-2z"/>
</svg>
`,
  // original: globe-16.svg
  FAVICON_GLOBE: `
<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
  <path fill="context-fill" d="M8 0a8 8 0 1 0 8 8 8.009 8.009 0 0 0-8-8zm5.163 4.958h-1.552a7.7 7.7 0 0 0-1.051-2.376 6.03 6.03 0 0 1 2.603 2.376zM14 8a5.963 5.963 0 0 1-.335 1.958h-1.821A12.327 12.327 0 0 0 12 8a12.327 12.327 0 0 0-.156-1.958h1.821A5.963 5.963 0 0 1 14 8zm-6 6c-1.075 0-2.037-1.2-2.567-2.958h5.135C10.037 12.8 9.075 14 8 14zM5.174 9.958a11.084 11.084 0 0 1 0-3.916h5.651A11.114 11.114 0 0 1 11 8a11.114 11.114 0 0 1-.174 1.958zM2 8a5.963 5.963 0 0 1 .335-1.958h1.821a12.361 12.361 0 0 0 0 3.916H2.335A5.963 5.963 0 0 1 2 8zm6-6c1.075 0 2.037 1.2 2.567 2.958H5.433C5.963 3.2 6.925 2 8 2zm-2.56.582a7.7 7.7 0 0 0-1.051 2.376H2.837A6.03 6.03 0 0 1 5.44 2.582zm-2.6 8.46h1.549a7.7 7.7 0 0 0 1.051 2.376 6.03 6.03 0 0 1-2.603-2.376zm7.723 2.376a7.7 7.7 0 0 0 1.051-2.376h1.552a6.03 6.03 0 0 1-2.606 2.376z"/>
</svg>
`,

  recentEffectiveFavIcons: [],
  recentUneffectiveFavIcons: [],
  maxRecentEffectiveFavIcons: 30,
  effectiveFavIcons: new Map(),
  uneffectiveFavIcons: new Map(),
  tasks: [],
  processStep: 5,
  FAVICON_SIZE: 16,

  init() {
    this.onTabCreated = this.onTabCreated.bind(this);
    browser.tabs.onCreated.addListener(this.onTabCreated);

    this.onTabUpdated = this.onTabUpdated.bind(this);
    browser.tabs.onUpdated.addListener(this.onTabUpdated);

    this.onTabRemoved = this.onTabRemoved.bind(this);
    browser.tabs.onRemoved.addListener(this.onTabRemoved);

    this.onTabDetached = this.onTabDetached.bind(this);
    browser.tabs.onDetached.addListener(this.onTabDetached);

    this.canvas = document.createElement('canvas');
    this.canvas.width = this.canvas.height = this.FAVICON_SIZE;
    this.canvas.setAttribute('style', `
      visibility: hidden;
      pointer-events: none;
      position: fixed
    `);
    document.body.appendChild(this.canvas);

    window.addEventListener('unload', () => {
      browser.tabs.onCreated.removeListener(this.onTabCreated);
      browser.tabs.onUpdated.removeListener(this.onTabUpdated);
      browser.tabs.onRemoved.removeListener(this.onTabRemoved);
      browser.tabs.onDetached.removeListener(this.onTabDetached);
    }, { once: true });
  },

  sessionAPIAvailable: (
    browser.sessions &&
    browser.sessions.getTabValue &&
    browser.sessions.setTabValue &&
    browser.sessions.removeTabValue
  ),

  addTask(task) {
    this.tasks.push(task);
    this.run();
  },

  run() {
    if (this.running)
      return;
    this.running = true;
    const processOneTask = () => {
      if (this.tasks.length == 0) {
        this.running = false;
      }
      else {
        const tasks = this.tasks.splice(0, this.processStep);
        while (tasks.length > 0) {
          tasks.shift()();
        }
        window.requestAnimationFrame(processOneTask);
      }
    };
    processOneTask();
  },

  loadToImage(params = {}) {
    this.addTask(() => {
      this.getEffectiveURL(params.tab, params.url)
        .then(url => {
          params.image.src = url;
          params.image.classList.remove('error');
        },
              _aError => {
                params.image.src = '';
                params.image.classList.add('error');
              });
    });
  },

  maybeImageTab(_tab) { // for backward compatibility
    return false;
  },

  getSafeFaviconUrl(url) {
    switch (url) {
      case 'chrome://browser/content/aboutlogins/icons/favicon.svg':
        return this.getSVGDataURI(this.FAVICON_LOCKWISE);
      case 'chrome://browser/content/robot.ico':
        return this.FAVICON_ROBOT;
      case 'chrome://browser/skin/controlcenter/dashboard.svg':
        return this.getSVGDataURI(this.FAVICON_DASHBOARD);
      case 'chrome://browser/skin/developer.svg':
        return this.getSVGDataURI(this.FAVICON_DEVELOPER);
      case 'chrome://browser/skin/privatebrowsing/favicon.svg':
        return this.getSVGDataURI(this.FAVICON_PRIVATE_BROWSING);
      case 'chrome://browser/skin/settings.svg':
        return this.getSVGDataURI(this.FAVICON_SETTINGS);
      case 'chrome://browser/skin/window.svg':
        return this.getSVGDataURI(this.FAVICON_WINDOW);
      case 'chrome://devtools/skin/images/profiler-stopwatch.svg':
        return this.getSVGDataURI(this.FAVICON_PROFILER);
      case 'chrome://global/skin/icons/performance.svg':
        return this.getSVGDataURI(this.FAVICON_PERFORMANCE);
      case 'chrome://global/skin/icons/warning.svg':
        return this.getSVGDataURI(this.FAVICON_WARNING);
      case 'chrome://mozapps/skin/extensions/extensionGeneric-16.svg':
        return this.getSVGDataURI(this.FAVICON_EXTENSION);
      default:
        if (/^chrome:\/\//.test(url) &&
            !/^chrome:\/\/branding\//.test(url))
          return this.getSVGDataURI(this.FAVICON_GLOBE);
        break;
    }
    return url;
  },
  getSVGDataURI(svg) {
    return `data:image/svg+xml,${encodeURIComponent(svg.trim())}`;
  },

  async getLastEffectiveFavIconURL(tab) {
    let lastData = this.effectiveFavIcons.get(tab.id);
    if (lastData === undefined && this.sessionAPIAvailable) {
      lastData = await browser.sessions.getTabValue(tab.id, this.LAST_EFFECTIVE_FAVICON);
      this.effectiveFavIcons.set(tab.id, lastData || null);   // NOTE: null is valid cache entry here
    }
    if (lastData &&
        lastData.url == tab.url)
      return lastData.favIconUrl;
    return null;
  },

  getEffectiveURL(tab, url = null) {
    return new Promise(async (aResolve, aReject) => {
      url = this.getSafeFaviconUrl(url || tab.favIconUrl);
      if (!url && tab.discarded) {
        // discarded tab doesn't have favIconUrl, so we should use cached data.
        let lastData = this.effectiveFavIcons.get(tab.id);
        if (!lastData &&
            this.sessionAPIAvailable)
          lastData = await browser.sessions.getTabValue(tab.id, this.LAST_EFFECTIVE_FAVICON);
        if (lastData &&
            lastData.url == tab.url)
          url = lastData.favIconUrl;
      }

      let loader, onLoad, onError;
      const clear = (() => {
        if (loader) {
          loader.removeEventListener('load', onLoad, { once: true });
          loader.removeEventListener('error', onError, { once: true });
        }
        loader = onLoad = onError = undefined;
      });

      onLoad = ((cache) => {
        const uneffectiveIndex = this.recentUneffectiveFavIcons.indexOf(url);
        if (uneffectiveIndex > -1)
          this.recentUneffectiveFavIcons.splice(uneffectiveIndex, 1);
        const effectiveIndex = this.recentEffectiveFavIcons.findIndex(item => item && item.url === url);
        if (effectiveIndex > -1) {
          this.recentEffectiveFavIcons.splice(effectiveIndex, 1);
        }
        else {
          let data = null;
          if (this.DRAWABLE_FAVICON_PATTERN.test(url)) {
            const context = this.canvas.getContext('2d');
            context.clearRect(0, 0, this.FAVICON_SIZE, this.FAVICON_SIZE);
            context.drawImage(loader, 0, 0, this.FAVICON_SIZE, this.FAVICON_SIZE);
            try {
              data = this.canvas.toDataURL('image/png');
            }
            catch(_e) {
              // it can fail due to security reasons
            }
          }
          cache = {
            url,
            data
          };
        }
        this.recentEffectiveFavIcons.push(cache);
        this.recentEffectiveFavIcons = this.recentEffectiveFavIcons.slice(-this.maxRecentEffectiveFavIcons);

        const oldData = this.effectiveFavIcons.get(tab.id);
        if (!oldData ||
            oldData.url != tab.url ||
            oldData.favIconUrl != url) {
          const lastEffectiveFavicon = {
            url:        tab.url,
            favIconUrl: url
          };
          this.effectiveFavIcons.set(tab.id, lastEffectiveFavicon);
          if (this.sessionAPIAvailable)
            browser.sessions.setTabValue(tab.id, this.LAST_EFFECTIVE_FAVICON, lastEffectiveFavicon);
        }
        this.uneffectiveFavIcons.delete(tab.id);
        aResolve(cache && cache.data || url);
        clear();
      });
      onError = (async (aError) => {
        const effectiveIndex = this.recentEffectiveFavIcons.findIndex(item => item && item.url === url);
        if (effectiveIndex > -1)
          this.recentEffectiveFavIcons.splice(effectiveIndex, 1);
        const uneffectiveIndex = this.recentUneffectiveFavIcons.indexOf(url);
        if (uneffectiveIndex > -1)
          this.recentUneffectiveFavIcons.splice(uneffectiveIndex, 1);
        this.recentUneffectiveFavIcons.push(url);
        this.recentUneffectiveFavIcons = this.recentUneffectiveFavIcons.slice(-this.maxRecentEffectiveFavIcons);

        clear();
        const effectiveFaviconData = this.effectiveFavIcons.get(tab.id) ||
                                   (this.sessionAPIAvailable &&
                                    await browser.sessions.getTabValue(tab.id, this.LAST_EFFECTIVE_FAVICON));
        this.effectiveFavIcons.delete(tab.id);
        if (this.sessionAPIAvailable)
          browser.sessions.removeTabValue(tab.id, this.LAST_EFFECTIVE_FAVICON);
        if (!this.uneffectiveFavIcons.has(tab.id) &&
            effectiveFaviconData &&
            effectiveFaviconData.url == tab.url &&
            effectiveFaviconData.favIconUrl &&
            url != effectiveFaviconData.favIconUrl) {
          this.getEffectiveURL(tab, effectiveFaviconData.favIconUrl).then(aResolve, aError => {
            aReject(aError);
          });
        }
        else {
          this.uneffectiveFavIcons.set(tab.id, {
            url:        tab.url,
            favIconUrl: url
          });
          aReject(aError || new Error('No effective icon'));
        }
      });
      const foundCache = this.recentEffectiveFavIcons.find(item => item && item.url === url);
      if (foundCache)
        return onLoad(foundCache);
      if (!url ||
          !this.VALID_FAVICON_PATTERN.test(url) ||
          this.recentUneffectiveFavIcons.includes(url)) {
        onError();
        return;
      }
      loader = new Image();
      if (/^https?:/.test(url))
        loader.crossOrigin = 'anonymous';
      loader.addEventListener('load', () => onLoad(), { once: true });
      loader.addEventListener('error', onError, { once: true });
      try {
        loader.src = url;
      }
      catch(e) {
        onError(e);
      }
    });
  },

  onTabCreated(tab) {
    this.getEffectiveURL(tab).catch(_e => {});
  },

  onTabUpdated(tabId, changeInfo, _tab) {
    if (!this._hasFavIconInfo(changeInfo))
      return;
    let timer = this._updatingTabs.get(tabId);
    if (timer)
      clearTimeout(timer);
    // Updating of last effective favicon must be done after the loading
    // of the tab itself is correctly done, to avoid cookie problems on
    // some websites.
    // See also: https://github.com/piroor/treestyletab/issues/2064
    timer = setTimeout(async () => {
      this._updatingTabs.delete(tabId);
      const tab = await browser.tabs.get(tabId);
      if (!tab ||
          (changeInfo.favIconUrl &&
           tab.favIconUrl != changeInfo.favIconUrl) ||
          (changeInfo.url &&
           tab.url != changeInfo.url) ||
          !this._hasFavIconInfo(tab))
        return; // expired
      this.getEffectiveURL(
        tab,
        changeInfo.favIconUrl
      ).catch(_e => {});
    }, 5000);
    this._updatingTabs.set(tabId, timer);
  },
  _hasFavIconInfo(tabOrChangeInfo) {
    return 'favIconUrl' in tabOrChangeInfo;
  },
  _updatingTabs: new Map(),

  onTabRemoved(tabId, _removeInfo) {
    this.effectiveFavIcons.delete(tabId);
    this.uneffectiveFavIcons.delete(tabId);
  },

  onTabDetached(tabId, _detachInfo) {
    this.effectiveFavIcons.delete(tabId);
    this.uneffectiveFavIcons.delete(tabId);
  }
};
TabFavIconHelper.init();
export default TabFavIconHelper;
