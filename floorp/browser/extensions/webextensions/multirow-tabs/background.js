/* global browser */
import w3color from './w3color.js';

const NS_XHTML = 'http://www.w3.org/1999/xhtml';
const globalSheet = browser.runtime.getURL('browser.css');
export let defaultOptions = {
  enableIconColors: false,
  displayNewtab: true,
  displayTitlebar: true,
  displayPlaceholders: false,
  displayCloseButton: true,
  forceCompact: true,
  font: '',
  tabSize: 10,
  minTabSize: 150,
  maxTabSize: 300,
  minTabHeight: 26,
  maxTabRows: 99,
  minLightness: 59,
  maxLightness: 100,
  fitLightness: true,
  userCSS: '',
  userCSSCode: '',
};

let cachedOptions = {};
// In currentOptionsSheet we keep track of the dynamic stylesheet that applies
// options (such as the user font), so that we can unload() the sheet once the
// options change.
let currentOptionsSheet = '';
let iconSheets = new Map();

function makeDynamicSheet(options) {
  // User options are applied via a dynamic stylesheet. Doesn't look elegant
  // but keeps the API small.
  let rules = `
    @import url('${options.userCSS}');
    @import url('data:text/css;base64,${btoa(options.userCSSCode)}');
    @namespace url('http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul');
    #navigator-toolbox {
      ${options.font ? `--paxmod-font: ${options.font};` : ''}
      --paxmod-tab-size: ${options.tabSize}%;
      --paxmod-min-tab-size: ${options.minTabSize}px;
      --paxmod-max-tab-size: ${options.maxTabSize}px;
      --tab-min-height: ${options.minTabHeight}px !important;
      --paxmod-max-tab-rows: ${options.maxTabRows} !important;
      --paxmod-display-newtab: ${options.displayNewtab ? '-webkit-box' : 'none'};
      --paxmod-titlebar-display: ${options.displayTitlebar ? '-webkit-box' : 'none'};
      --paxmod-titlebar-placeholders: ${options.displayPlaceholders ? '1000px' : '0px'};
      --paxmod-display-close-button: ${options.displayCloseButton ? '-webkit-box' : 'none'};
      ${options.forceCompact && '--proton-tab-block-margin: 0; --tab-block-margin: 0;'}
    }
    ${options.forceCompact && `
      .tabbrowser-tab {
        padding-inline: 0 !important;
      }
    `}
  `;
  // -webkit-box is used as a replacement for -moz-box which doesn't seem to
  // work in FF >= 63. That's possibly an internal bug.

  // CSS rules are base64-encoded because the native StyleSheetService API
  // can't handle some special chars.
  return `data:text/css;base64,${btoa(rules)}`;
}

// Return a version of SVG `data:` URL `url` with specific dimensions
//
// Needed because an SVG without specified width/height appears to have zero
// size and can't be drawn on a canvas which we need to extract image data.
function patchSVGDataURL(url) {
  let code = atob(url.split( ',', 2)[1]);
  let dom = (new DOMParser()).parseFromString(code, 'image/svg+xml');
  if (dom.documentElement.nodeName !== 'svg') {
    // May happen on XML parsing errors
    throw 'Failed to build SVG DOM';
  }
  dom.documentElement.setAttribute('width', '64');
  dom.documentElement.setAttribute('height', '64');
  code = (new XMLSerializer()).serializeToString(dom);
  return `data:image/svg+xml;base64,${btoa(code)}`;
}

async function addIconColor(url, urlOrig = null) {
  if ((!url) || url.startsWith('chrome://') || url.includes('\'') || iconSheets.has(url)) {
    return;
  }
  let img = document.createElementNS(NS_XHTML, 'img');
  img.addEventListener('load', () => {
    if (!urlOrig && img.width == 0 && url.startsWith('data:')) {
      addIconColor(patchSVGDataURL(url), url);
      return;
    }
    let color = iconColor(
      img,
      Number(cachedOptions.minLightness),
      Number(cachedOptions.maxLightness),
    );
    // We can't access the chrome DOM, so apply each favicon color via stylesheet
    let sheetText = `data:text/css,tab.tabbrowser-tab[image='${urlOrig || url}'] .tab-label { color: ${color} !important; }`;
    browser.stylesheet.load(sheetText, 'AUTHOR_SHEET');
    iconSheets.set(urlOrig || url, sheetText);
    img.remove();
  });
  img.src = url;
}

async function addAllIconColors() {
  let tabs = await browser.tabs.query({});
  for (let tab of tabs) {
    if (tab.favIconUrl) {
      addIconColor(tab.favIconUrl);
    }
  }
}

function removeAllIconColors() {
  for (let sheet of iconSheets.values()) {
    browser.stylesheet.unload(sheet, 'AUTHOR_SHEET');
  }
  iconSheets.clear();
}

export async function getOptions() {
  return await browser.storage.local.get();
}

export async function setOptions(options) {
  await browser.storage.local.set(options);
}

export async function applyOptions() {
  let options = await getOptions();
  let theme = await browser.theme.getCurrent();
  if (options.fitLightness !== false) {
    Object.assign(options, getBestLightnessOptions(theme));
  }
  removeAllIconColors();
  cachedOptions.minLightness = options.minLightness;
  cachedOptions.maxLightness = options.maxLightness;

  let newOptionsSheet = makeDynamicSheet(options);
  if (currentOptionsSheet) {
    await browser.stylesheet.unload(currentOptionsSheet, 'AUTHOR_SHEET');
  }
  await browser.stylesheet.load(newOptionsSheet, 'AUTHOR_SHEET');
  currentOptionsSheet = newOptionsSheet;
  if (options.enableIconColors) {
    if (!browser.tabs.onUpdated.hasListener(onFavIconChanged)) {
      browser.tabs.onUpdated.addListener(onFavIconChanged, {properties: ["favIconUrl"]});
    }
    await addAllIconColors();
  } else {
    if (browser.tabs.onUpdated.hasListener(onFavIconChanged)) {
      browser.tabs.onUpdated.removeListener(onFavIconChanged);
    }
  }
}

function onFavIconChanged(tabId, changeInfo) {
  addIconColor(changeInfo.favIconUrl);
}

// Return the best tab lightness settings for a given theme
export function getBestLightnessOptions(theme) {
  // Maps theme color properties to whether their lightness corresponds to the
  // inverted theme lightness, ordered by significance
  let invertColorMap = {
    tab_text: true,
    textcolor: true,
    tab_selected: false,
    frame: false,
    accentcolor: false,
    bookmark_text: true,
    toolbar_text: true,
  };
  let light = {
    minLightness: 0,
    maxLightness: 52,
  };
  let dark = {
      minLightness: 59,
      maxLightness: 100,
  };
  let colors = theme.colors;
  if (!colors) {
    return light;
  }
  for (let prop in invertColorMap) {
    if (!colors[prop]) {
      continue;
    }
    return (w3color(colors[prop]).isDark() !== invertColorMap[prop]) ? dark : light;
  }
  return light;
}

async function startup() {
  if (!('paxmod' in browser && 'stylesheet' in browser)) {
    browser.tabs.create({url: browser.runtime.getURL('missing_api.html')});
  }

  browser.stylesheet.load(globalSheet, 'AUTHOR_SHEET');
  browser.paxmod.load();
  let options = await getOptions();
  let newOptions = {};
  for (let key in defaultOptions) {
    if (!(key in options)) {
      newOptions[key] = defaultOptions[key];
    }
  }
  if (Object.keys(newOptions).length > 0) {
    await setOptions(newOptions);
  }
  applyOptions();

  // When idiling, occasionally check if icon sheets should be refreshed to
  // avoid growing a large cache.
  browser.idle.setDetectionInterval(60 * 10);
  browser.idle.onStateChanged.addListener(async (state) => {
    if (state === 'idle' && iconSheets.size > 1000 &&
        iconSheets.size > (await browser.tabs.query({})).length * 1.5) {
      removeAllIconColors();
      await addAllIconColors();
    }
  });
}

(async() => {
  let enabled = await browser.aboutConfigPrefs.getPref("floorp.tabbar.style");
  if (enabled == 1) {
    browser.theme.onUpdated.addListener(async details => {
      if ((await browser.storage.local.get('fitLightness')).fitLightness !== false) {
        // Re-apply options, so the lightness settings fit the new theme
        applyOptions();
      }
    });
    
    startup();
  }
  browser.aboutConfigPrefs.onPrefChange.addListener(function(){
    browser.runtime.reload();
  }, "floorp.tabbar.style");
})();
