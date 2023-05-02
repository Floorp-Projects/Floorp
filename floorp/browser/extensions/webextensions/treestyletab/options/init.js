/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import Options from '/extlib/Options.js';
import '/extlib/l10n.js';
import RichConfirm from '/extlib/RichConfirm.js';

import {
  DEVICE_SPECIFIC_CONFIG_KEYS,
  log,
  wait,
  configs,
  sanitizeForHTMLText,
  loadUserStyleRules,
  saveUserStyleRules,
  sanitizeAccesskeyMark,
} from '/common/common.js';

import * as Constants from '/common/constants.js';
import * as Permissions from '/common/permissions.js';
import * as Bookmark from '/common/bookmark.js';
import * as BrowserTheme from '/common/browser-theme.js';
import * as TSTAPI from '/common/tst-api.js';
import * as ApiTabs from '/common/api-tabs.js';
import * as Sync from '/common/sync.js';

log.context = 'Options';

const options = new Options(configs, {
  steps: {
    faviconizedTabScale: '0.01'
  },
  onImporting(values) {
    for (const key of DEVICE_SPECIFIC_CONFIG_KEYS) {
      if (JSON.stringify(configs[key]) != JSON.stringify(configs.$default[key]))
        values[key] = configs[key];
      else
        delete values[key];
    }
    return values;
  },
  onExporting(values) {
    for (const key of DEVICE_SPECIFIC_CONFIG_KEYS) {
      delete values[key];
    }
    return values;
  },
});

document.title = browser.i18n.getMessage('config_title');
if ((location.hash && location.hash != '#') ||
    /independent=true/.test(location.search))
  document.body.classList.add('independent');

const CODEMIRROR_THEMES = `
3024-day
3024-night
abcdef
ambiance-mobile
ambiance
ayu-dark
ayu-mirage
base16-dark
base16-light
bespin
blackboard
cobalt
colorforth
darcula
dracula
duotone-dark
duotone-light
eclipse
elegant
erlang-dark
gruvbox-dark
hopscotch
icecoder
idea
isotope
lesser-dark
liquibyte
lucario
material-darker
material-ocean
material-palenight
material
mbo
mdn-like
midnight
monokai
moxer
neat
neo
night
nord
oceanic-next
panda-syntax
paraiso-dark
paraiso-light
pastel-on-dark
railscasts
rubyblue
seti
shadowfox
solarized
ssms
the-matrix
tomorrow-night-bright
tomorrow-night-eighties
ttcn
twilight
vibrant-ink
xq-dark
xq-light
yeti
yonce
zenburn
`.trim().split(/\s+/);
{
  const range = document.createRange();
  range.selectNodeContents(document.querySelector('#userStyleRulesFieldTheme'));
  range.collapse(false);
  range.insertNode(range.createContextualFragment(CODEMIRROR_THEMES.map(theme => `
    <option value=${JSON.stringify(sanitizeForHTMLText(theme))}>${sanitizeForHTMLText(theme)}</option>
  `.trim()).join('')));
  range.detach();
}

const mUserStyleRulesField = document.getElementById('userStyleRulesField');
let mUserStyleRulesFieldEditor;

const mDarkModeMedia = window.matchMedia('(prefers-color-scheme: dark)');

function onConfigChanged(key) {
  const value = configs[key];
  switch (key) {
    case 'successorTabControlLevel': {
      const checkbox = document.getElementById('simulateSelectOwnerOnClose');
      const label = checkbox.parentNode;
      if (value == Constants.kSUCCESSOR_TAB_CONTROL_NEVER) {
        checkbox.setAttribute('disabled', true);
        label.setAttribute('disabled', true);
      }
      else {
        checkbox.removeAttribute('disabled');
        label.removeAttribute('disabled');
      }
    }; break;

    case 'parentTabOperationBehaviorMode': {
      const nodes = document.querySelectorAll('#parentTabOperationBehaviorModeGroup > ul > li > :not(label)');
      for (const node of nodes) {
        node.style.display = node.parentNode.querySelector('input[type="radio"]').checked ? '' : 'none';
        const chosen = node.querySelector(`[type="radio"][data-config-key="closeParentBehavior"][value="${configs.closeParentBehavior}"]`);
        if (chosen) {
          chosen.checked = true;
          continue;
        }
        const chooser = node.querySelector('[data-config-key="closeParentBehavior"]');
        if (chooser)
          chooser.value = configs.closeParentBehavior;
      }
    }; break;

    case 'autoAttachOnAnyOtherTrigger': {
      const nodes = document.querySelectorAll('.sub.autoAttachOnAnyOtherTrigger label, .sub.autoAttachOnAnyOtherTrigger select');
      const disabled = configs.autoAttachOnAnyOtherTrigger == Constants.kNEWTAB_DO_NOTHING;
      for (const node of nodes) {
        if ('disabled' in node)
          node.disabled = disabled;
        else
          node.setAttribute('disabled', disabled);
      }
    }; break;

    case 'showExpertOptions':
      document.documentElement.classList.toggle('show-expert-options', configs.showExpertOptions);
      break;

    case 'syncDeviceInfo': {
      const name = (configs.syncDeviceInfo || {}).name || '';
      const nameField = document.querySelector('#syncDeviceInfoName');
      if (name != nameField.value)
        nameField.value = name;
      const icon = (configs.syncDeviceInfo || {}).icon || '';
      const iconRadio = document.querySelector(`#syncDeviceInfoIcon input[type="radio"][value=${JSON.stringify(sanitizeForHTMLText(icon))}]`);
      if (iconRadio && !iconRadio.checked)
        iconRadio.checked = true;
    }; break;

    case 'syncDevices':
      initOtherDevices();
      break;

    case 'userStyleRulesFieldTheme':
      applyUserStyleRulesFieldTheme();
      break;

    case 'userStyleRulesFieldHeight':
      mUserStyleRulesField.style.height = configs.userStyleRulesFieldHeight;
      break;

    default:
      if (key.startsWith('chunkedUserStyleRules') &&
          !mUserStyleRulesField.$saving)
        mUserStyleRulesFieldEditor.setValue(loadUserStyleRules());
      break;
  }
}

function removeAccesskeyMark(node) {
  if (!node.nodeValue)
    return;
  node.nodeValue = sanitizeAccesskeyMark(node.nodeValue);
}

function onChangeParentCheckbox(event) {
  const container = event.currentTarget.closest('fieldset');
  for (const checkbox of container.querySelectorAll('p input[type="checkbox"]')) {
    checkbox.checked = event.currentTarget.checked;
  }
  saveLogForConfig();
}

function onChangeChildCheckbox(event) {
  getParentCheckboxFromChild(event.currentTarget).checked = isAllChildrenChecked(event.currentTarget);
  saveLogForConfig();
}

function getParentCheckboxFromChild(child) {
  const container = child.closest('fieldset');
  return container.querySelector('legend input[type="checkbox"]');
}

async function onChangeBookmarkPermissionRequiredCheckboxState(event) {
  const permissionCheckbox = document.getElementById('bookmarksPermissionGranted');
  if (permissionCheckbox.checked)
    return;

  permissionCheckbox.checked = true;
  permissionCheckbox.requestPermissions();

  const checkbox = event.currentTarget;
  const key = checkbox.name || checkbox.id || checkbox.dataset.configKey;
  setTimeout(() => {
    checkbox.checked = true;
    setTimeout(() => {
      configs[key] = true;
    }, 300); // 250 msec is the minimum delay of throttle update
  }, 100);
}

function updateCtrlTabSubItems(enabled) {
  const elements = document.querySelectorAll('#ctrlTabSubItemsContainer label, #ctrlTabSubItemsContainer input, #ctrlTabSubItemsContainer select');
  if (enabled) {
    for (const element of elements) {
      element.removeAttribute('disabled');
    }
  }
  else {
    for (const element of elements) {
      element.setAttribute('disabled', true);
    }
  }
}


function reserveToSaveUserStyleRules() {
  if (reserveToSaveUserStyleRules.timer)
    clearTimeout(reserveToSaveUserStyleRules.timer);
  reserveToSaveUserStyleRules.timer = setTimeout(() => {
    reserveToSaveUserStyleRules.timer = null;
    const caution = document.querySelector('#tooLargeUserStyleRulesCaution');
    mUserStyleRulesField.$saving = true;
    if (reserveToSaveUserStyleRules.clearFlagTimer)
      clearTimeout(reserveToSaveUserStyleRules.clearFlagTimer);
    try {
      saveUserStyleRules(mUserStyleRulesFieldEditor.getValue());
      mUserStyleRulesField.classList.remove('invalid');
      caution.classList.remove('invalid');
    }
    catch(_error) {
      mUserStyleRulesField.classList.add('invalid');
      caution.classList.add('invalid');
    }
    finally {
      reserveToSaveUserStyleRules.clearFlagTimer = setTimeout(() => {
        delete reserveToSaveUserStyleRules.clearFlagTimer;
        mUserStyleRulesField.$saving = false;
      }, 1000);
    }
  }, 250);
}
reserveToSaveUserStyleRules.timer = null;

function getUserStyleRulesFieldTheme() {
  if (configs.userStyleRulesFieldTheme != 'auto')
    return configs.userStyleRulesFieldTheme;

  return mDarkModeMedia.matches ? 'bespin' : 'default';
}

function applyUserStyleRulesFieldTheme() {
  const theme = getUserStyleRulesFieldTheme();
  if (theme != 'default' &&
      !document.querySelector(`link[href$="/extlib/codemirror-theme/${theme}.css"]`)) {
    const range = document.createRange();
    range.selectNodeContents(document.querySelector('head'));
    range.collapse(false);
    range.insertNode(range.createContextualFragment(`
      <link rel="stylesheet"
            type="text/css"
            href="/extlib/codemirror-theme/${theme}.css"/>
    `.trim()));
    range.detach();
  }
  mUserStyleRulesFieldEditor.setOption('theme', theme);
}


function saveLogForConfig() {
  const config = {};
  for (const checkbox of document.querySelectorAll('p input[type="checkbox"][id^="logFor-"]')) {
    config[checkbox.id.replace(/^logFor-/, '')] = checkbox.checked;
  }
  configs.logFor = config;
}

function isAllChildrenChecked(aMasger) {
  const container = aMasger.closest('fieldset');
  const checkboxes = container.querySelectorAll('p input[type="checkbox"]');
  return Array.from(checkboxes).every(checkbox => checkbox.checked);
}

async function updateBookmarksUI(enabled) {
  const elements = document.querySelectorAll('.with-bookmarks-permission, .with-bookmarks-permission label, .with-bookmarks-permission input, .with-bookmarks-permission button');
  if (enabled) {
    for (const element of elements) {
      element.removeAttribute('disabled');
    }
    const defaultParentFolder = (
      (await Bookmark.getItemById(configs.defaultBookmarkParentId)) ||
      (await Bookmark.getItemById(configs.$defaults.defaultBookmarkParentId))
    );
    const defaultBookmarkParentChooser = document.getElementById('defaultBookmarkParentChooser');
    Bookmark.initFolderChooser(defaultBookmarkParentChooser, {
      defaultValue: defaultParentFolder.id,
      onCommand:    (item, _event) => {
        if (item.dataset.id)
          configs.defaultBookmarkParentId = item.dataset.id;
      },
      rootItems: (await browser.bookmarks.getTree().catch(ApiTabs.createErrorHandler()))[0].children,
      incrementalSearchTimeout: configs.incrementalSearchTimeout,
    });
  }
  else {
    for (const element of elements) {
      element.setAttribute('disabled', true);
    }
  }

  const triboolChecks = document.querySelectorAll('.require-bookmarks-permission');
  if (enabled) {
    for (const checkbox of triboolChecks) {
      checkbox.classList.remove('missing-permission');
      const message = checkbox.dataset.requestPermissionMessage;
      if (message && checkbox.parentNode.getAttribute('title') == message)
        checkbox.parentNode.removeAttribute('title');
    }
  }
  else {
    for (const checkbox of triboolChecks) {
      checkbox.classList.add('missing-permission');
      const message = checkbox.dataset.requestPermissionMessage;
      if (message)
        checkbox.parentNode.setAttribute('title', message);
    }
  }
}

async function initOtherDevices() {
  await Sync.waitUntilDeviceInfoInitialized();
  const container = document.querySelector('#otherDevices');
  const range = document.createRange();
  range.selectNodeContents(container);
  range.deleteContents();
  for (const device of Sync.getOtherDevices()) {
    const icon = device.icon ? `<img src="/resources/icons/${sanitizeForHTMLText(device.icon)}.svg">` : '';
    const contents = range.createContextualFragment(`
      <li id="otherDevice:${sanitizeForHTMLText(String(device.id))}"
         ><label>${icon}${sanitizeForHTMLText(String(device.name))}
                 <button title=${JSON.stringify(sanitizeForHTMLText(browser.i18n.getMessage('config_removeDeviceButton_label')))}
                        >${sanitizeForHTMLText(browser.i18n.getMessage('config_removeDeviceButton_label'))}</button></label></li>
    `.trim());
    range.insertNode(contents);
  }
  range.detach();
}

function removeOtherDevice(id) {
  const devices = JSON.parse(JSON.stringify(configs.syncDevices));
  if (!(id in devices))
    return;
  delete devices[id];
  configs.syncDevices = devices;
}


async function showLogs() {
  browser.tabs.create({
    url: '/resources/logs.html'
  });
}

function initUserStyleImportExportButtons() {
  const exportButton = document.getElementById('userStyleRules-export');
  exportButton.addEventListener('keydown', event => {
    if (event.key == 'Enter' || event.key == ' ')
      exportUserStyleToFile();
  });
  exportButton.addEventListener('click', event => {
    if (event.button == 0)
      exportUserStyleToFile();
  });

  const importButton = document.getElementById('userStyleRules-import');
  importButton.addEventListener('keydown', event => {
    if (event.key == 'Enter' || event.key == ' ')
      document.getElementById('userStyleRules-import-file').click();
  });
  importButton.addEventListener('click', event => {
    if (event.button == 0)
      document.getElementById('userStyleRules-import-file').click();
  });

  const fileField = document.getElementById('userStyleRules-import-file');
  fileField.addEventListener('change', async _event => {
    importFilesToUserStyleRulesField(fileField.files);
  });
}

function exportUserStyleToFile() {
  const styleRules = mUserStyleRulesFieldEditor.getValue();
  const link = document.getElementById('userStyleRules-export-file');
  link.href = URL.createObjectURL(new Blob([styleRules], { type: 'text/css' }));
  link.click();
}

// Due to https://bugzilla.mozilla.org/show_bug.cgi?id=1408756 we cannot accept dropped files on an embedded options page...
function initFileDragAndDropHandlers() {
  mUserStyleRulesField.addEventListener('dragenter', event => {
    event.stopPropagation();
    event.preventDefault();
  }, { capture: true });

  mUserStyleRulesField.addEventListener('dragover', event => {
    event.stopPropagation();
    event.preventDefault();

    const dt = event.dataTransfer;
    const hasFile = Array.from(dt.items, item => item.kind).some(kind => kind == 'file');
    dt.dropEffect = hasFile ? 'link' : 'none';
  }, { capture: true });

  mUserStyleRulesField.addEventListener('drop', event => {
    event.stopPropagation();
    event.preventDefault();

    const dt = event.dataTransfer;
    const files = dt.files;
    if (files && files.length > 0)
      importFilesToUserStyleRulesField(files);
  }, { capture: true });
}

async function importFilesToUserStyleRulesField(files) {
  files = Array.from(files);
  if (files.some(file => file.type.startsWith('image/'))) {
    const contents = await Promise.all(Array.from(files, file => {
      switch (file.type) {
        case 'text/plain':
        case 'text/css':
          return file.text();

        default:
          return new Promise((resolve, reject) => {
            const reader = new FileReader();
            reader.addEventListener('load', () => {
              resolve(`url(${JSON.stringify(reader.result)})`);
            });
            reader.addEventListener('error', event => {
              reject(event);
            });
            reader.readAsDataURL(file);
          });
      }
    }));
    mUserStyleRulesFieldEditor.replaceSelection(contents.join('\n'), true);
  }
  else {
    const style = (await Promise.all(files.map(file => file.text()))).join('\n');
    const current = mUserStyleRulesFieldEditor.getValue().trim();
    if (current == '') {
      mUserStyleRulesFieldEditor.setValue(style);
      return;
    }
    let result;
    try {
      result = await RichConfirm.showInPopup({
        modal:   true,
        type:    'common-dialog',
        url:     '/resources/blank.html', // required on Firefox ESR68
        title:   browser.i18n.getMessage('config_userStyleRules_overwrite_title'),
        message: browser.i18n.getMessage('config_userStyleRules_overwrite_message'),
        buttons: [
          browser.i18n.getMessage('config_userStyleRules_overwrite_overwrite'),
          browser.i18n.getMessage('config_userStyleRules_overwrite_append')
        ]
      });
    }
    catch(_error) {
      result = { buttonIndex: -1 };
    }
    switch (result.buttonIndex) {
      case 0:
        mUserStyleRulesFieldEditor.setValue(style);
        break;
      case 1:
        mUserStyleRulesFieldEditor.setValue(`${current}\n${style}`);
        break;
      default:
        break;
    }
  }
  mUserStyleRulesField.focus();
}

function updateThemeInformation(theme) {
  const rules = BrowserTheme.generateThemeRules(theme)
    .replace(/--theme-[^:]*-[0-9]+:[^;]*;\s*/g, '') /* hide alpha variations */
    .replace(/(#(?:[0-9a-f]{3,8})|(?:rgb|hsl)a?\([^\)]+\))/gi, `$1<span style="
      background-color: $1;
      border-radius:    0.2em;
      box-shadow:       1px 1px 1.5px black;
      display:          inline-block;
      height:           1em;
      width:            1em;
    ">\u200b</span>`);
  const range = document.createRange();
  range.selectNodeContents(document.getElementById('browserThemeCustomRules'));
  range.deleteContents();
  range.insertNode(range.createContextualFragment(rules));
  range.detach();
  document.getElementById('browserThemeCustomRulesBlock').style.display = rules ? 'block' : 'none';
}

function autoDetectDuplicatedTabDetectionDelay() {
  browser.runtime.sendMessage({ type: Constants.kCOMMAND_AUTODETECT_DUPLICATED_TAB_DETECTION_DELAY });
}

async function testDuplicatedTabDetection() {
  const successRate = await browser.runtime.sendMessage({ type: Constants.kCOMMAND_TEST_DUPLICATED_TAB_DETECTION });
  document.querySelector('#delayForDuplicatedTabDetection_testResult').textContent = browser.i18n.getMessage('config_delayForDuplicatedTabDetection_test_resultMessage', [successRate * 100]);
}


configs.$addObserver(onConfigChanged);
window.addEventListener('DOMContentLoaded', async () => {
  try {
    document.documentElement.classList.toggle('successor-tab-support', typeof browser.tabs.moveInSuccession == 'function');

    initAccesskeys();
    initLogsButton();
    initDuplicatedTabDetection();
    initLinks();
    initTheme();
  }
  catch(error) {
    console.error(error);
  }

  await configs.$loaded;

  let focusedItem;
  try {
    focusedItem = initFocusedItem();
    initCollapsibleSections({ focusedItem });
    initPermissionOptions();
    initLogCheckboxes();
    initPreviews();
    initExternalAddons();
    initSync();
  }
  catch(error) {
    console.error(error);
  }

  try {
    options.buildUIForAllConfigs(document.querySelector('#group-allConfigs'));
    onConfigChanged('successorTabControlLevel');
    onConfigChanged('showExpertOptions');
    await wait(0);
    onConfigChanged('parentTabOperationBehaviorMode');
    onConfigChanged('autoAttachOnAnyOtherTrigger');
    onConfigChanged('syncDeviceInfo');

    if (focusedItem)
      focusedItem.scrollIntoView({ block: 'start' });
  }
  catch(error) {
    console.error(error);
  }

  document.documentElement.classList.add('initialized');
}, { once: true });

function initAccesskeys() {
  for (const label of document.querySelectorAll('.contextConfigs label')) {
    for (const child of label.childNodes) {
      if (child.nodeType == Node.TEXT_NODE)
        removeAccesskeyMark(child);
    }
  }
}

function initLogsButton() {
  const showLogsButton = document.getElementById('showLogsButton');
  showLogsButton.addEventListener('click', event => {
    if (event.button != 0)
      return;
    showLogs();
  });
  showLogsButton.addEventListener('keydown', event => {
    if (event.key != 'Enter')
      return;
    showLogs();
  });
}

function initDuplicatedTabDetection() {
  const autoDetectDuplicatedTabDetectionDelayButton = document.getElementById('delayForDuplicatedTabDetection_autoDetectButton');
  autoDetectDuplicatedTabDetectionDelayButton.addEventListener('click', event => {
    if (event.button != 0)
      return;
    autoDetectDuplicatedTabDetectionDelay();
  });
  autoDetectDuplicatedTabDetectionDelayButton.addEventListener('keydown', event => {
    if (event.key != 'Enter')
      return;
    autoDetectDuplicatedTabDetectionDelay();
  });

  const testDuplicatedTabDetectionButton = document.getElementById('delayForDuplicatedTabDetection_testButton');
  testDuplicatedTabDetectionButton.addEventListener('click', event => {
    if (event.button != 0)
      return;
    testDuplicatedTabDetection();
  });
  testDuplicatedTabDetectionButton.addEventListener('keydown', event => {
    if (event.key != 'Enter')
      return;
    testDuplicatedTabDetection();
  });
}

function initLinks() {
  document.getElementById('link-optionsPage-top').setAttribute('href', `${location.href.split('#')[0]}#!`);
  document.getElementById('link-optionsPage').setAttribute('href', `${location.href.split('#')[0]}#!`);
  document.getElementById('link-startupPage').setAttribute('href', Constants.kSHORTHAND_URIS.startup);
  document.getElementById('link-groupPage').setAttribute('href', Constants.kSHORTHAND_URIS.group);
  document.getElementById('link-tabbarPage').setAttribute('href', Constants.kSHORTHAND_URIS.tabbar);
  document.getElementById('link-runTests').setAttribute('href', Constants.kSHORTHAND_URIS.testRunner);
  document.getElementById('link-runBenchmark').setAttribute('href', `${Constants.kSHORTHAND_URIS.testRunner}?benchmark=true`);
}

function initTheme() {
  if (browser.theme && browser.theme.getCurrent) {
    browser.theme.getCurrent().then(updateThemeInformation);
    browser.theme.onUpdated.addListener(updateInfo => updateThemeInformation(updateInfo.theme));
  }
}

function initFocusedItem() {
  const focusedItem = document.querySelector(':target');
  for (const fieldset of document.querySelectorAll('fieldset.collapsible')) {
    if (configs.optionsExpandedGroups.includes(fieldset.id) ||
        (focusedItem && fieldset.contains(focusedItem)))
      fieldset.classList.remove('collapsed');
    else
      fieldset.classList.add('collapsed');

    const onChangeCollapsed = () => {
      if (!fieldset.id)
        return;
      const otherExpandedSections = configs.optionsExpandedGroups.filter(id => id != fieldset.id);
      if (fieldset.classList.contains('collapsed'))
        configs.optionsExpandedGroups = otherExpandedSections;
      else
        configs.optionsExpandedGroups = otherExpandedSections.concat([fieldset.id]);
    };

    const legend = fieldset.querySelector(':scope > legend');
    legend.addEventListener('click', () => {
      fieldset.classList.toggle('collapsed');
      onChangeCollapsed();
    });
    legend.addEventListener('keydown', event => {
      if (event.key != 'Enter')
        return;
      fieldset.classList.toggle('collapsed');
      onChangeCollapsed();
    });
  }

  return focusedItem;
}

function initCollapsibleSections({ focusedItem }) {
  for (const heading of document.querySelectorAll('body > section > h1')) {
    const section = heading.parentNode;
    section.style.maxHeight = `${heading.offsetHeight}px`;
    if (!configs.optionsExpandedSections.includes(section.id) &&
          (!focusedItem || !section.contains(focusedItem)))
      section.classList.add('collapsed');
    heading.addEventListener('click', () => {
      section.classList.toggle('collapsed');
      const otherExpandedSections = configs.optionsExpandedSections.filter(id => id != section.id);
      if (section.classList.contains('collapsed'))
        configs.optionsExpandedSections = otherExpandedSections;
      else
        configs.optionsExpandedSections = otherExpandedSections.concat([section.id]);
    });
  }
}

function initPermissionOptions() {
  Permissions.isGranted(Permissions.BOOKMARKS).then(granted => updateBookmarksUI(granted));
  Permissions.isGranted(Permissions.ALL_URLS).then(granted => updateCtrlTabSubItems(granted));

  Permissions.bindToCheckbox(
    Permissions.ALL_URLS,
    document.querySelector('#allUrlsPermissionGranted_ctrlTabTracking'),
    {
      onChanged: (granted) => {
        configs.skipCollapsedTabsForTabSwitchingShortcuts = granted;
        updateCtrlTabSubItems(granted);
      }
    }
  );
  Permissions.bindToCheckbox(
    Permissions.BOOKMARKS,
    document.querySelector('#bookmarksPermissionGranted'),
    { onChanged: (granted) => updateBookmarksUI(granted) }
  );
  Permissions.bindToCheckbox(
    Permissions.BOOKMARKS,
    document.querySelector('#bookmarksPermissionGranted_context'),
    { onChanged: (granted) => updateBookmarksUI(granted) }
  );
  Permissions.bindToCheckbox(
    Permissions.TAB_HIDE,
    document.querySelector('#tabHidePermissionGranted'),
    { onChanged: async (granted) => {
      if (granted) {
        // try to hide/show the tab to ensure the permission is really granted
        let activeTabs = await browser.tabs.query({ active: true, currentWindow: true });
        if (activeTabs.length == 0)
          activeTabs = await browser.tabs.query({ currentWindow: true });
        const tab = await browser.tabs.create({ active: false, windowId: activeTabs[0].windowId });
        await wait(200);
        let aborted = false;
        const onRemoved = tabId => {
          if (tabId != tab.id)
            return;
          aborted = true;
          browser.tabs.onRemoved.removeListener(onRemoved);
          // eslint-disable-next-line no-use-before-define
          browser.tabs.onUpdated.removeListener(onUpdated);
        };
        const onUpdated = async (tabId, changeInfo, tab) => {
          if (tabId != tab.id ||
                !('hidden' in changeInfo))
            return;
          await wait(60 * 1000);
          if (aborted)
            return;
          await browser.tabs.show([tab.id]);
          await browser.tabs.remove(tab.id);
        };
        browser.tabs.onRemoved.addListener(onRemoved);
        browser.tabs.onUpdated.addListener(onUpdated);
        await browser.tabs.hide([tab.id]);
      }
    }}
  );

  for (const checkbox of document.querySelectorAll('input[type="checkbox"].require-bookmarks-permission')) {
    checkbox.addEventListener('change', onChangeBookmarkPermissionRequiredCheckboxState);
  }
}

function initLogCheckboxes() {
  for (const checkbox of document.querySelectorAll('p input[type="checkbox"][id^="logFor-"]')) {
    checkbox.addEventListener('change', onChangeChildCheckbox);
    checkbox.checked = configs.logFor[checkbox.id.replace(/^logFor-/, '')];
  }
  for (const checkbox of document.querySelectorAll('legend input[type="checkbox"][id^="logFor-"]')) {
    checkbox.checked = isAllChildrenChecked(checkbox);
    checkbox.addEventListener('change', onChangeParentCheckbox);
  }
}

function initPreviews() {
  for (const previewImage of document.querySelectorAll('select ~ .preview-image')) {
    const container = previewImage.parentNode;
    container.classList.add('has-preview-image');
    const select = container.querySelector('select');
    container.dataset.value = select.dataset.value = select.value;
    container.addEventListener('mouseover', event => {
      if (event.target != select &&
            select.contains(event.target))
        return;
      const rect = select.getBoundingClientRect();
      previewImage.style.left = `${rect.left}px`;
      previewImage.style.top  = `${rect.top - 5 - previewImage.offsetHeight}px`;
    });
    select.addEventListener('change', () => {
      container.dataset.value = select.dataset.value = select.value;
    });
    select.addEventListener('mouseover', event => {
      if (event.target == select)
        return;
      container.dataset.value = select.dataset.value = event.target.value;
    });
    select.addEventListener('mouseout', () => {
      container.dataset.value = select.dataset.value = select.value;
    });
  }
}

async function initExternalAddons() {
  const addons = await browser.runtime.sendMessage({
    type: TSTAPI.kCOMMAND_GET_ADDONS
  });

  const description = document.getElementById('externalAddonPermissionsGroupDescription');
  const range = document.createRange();
  range.selectNodeContents(description);
  description.appendChild(range.createContextualFragment(browser.i18n.getMessage('config_externaladdonpermissions_description')));
  range.detach();

  const container = document.getElementById('externalAddonPermissions');
  for (const addon of addons) {
    if (addon.id == browser.runtime.id)
      continue;
    const row = document.createElement('tr');

    const nameCell = row.appendChild(document.createElement('td'));
    const nameLabel = nameCell.appendChild(document.createElement('label'));
    nameLabel.appendChild(document.createTextNode(addon.label));
    const controlledId = `api-permissions-${encodeURIComponent(addon.id)}`;
    nameLabel.setAttribute('for', controlledId);

    const incognitoCell = row.appendChild(document.createElement('td'));
    const incognitoLabel = incognitoCell.appendChild(document.createElement('label'));
    const incognitoCheckbox = incognitoLabel.appendChild(document.createElement('input'));
    if (addon.permissions.length == 0)
      incognitoCheckbox.setAttribute('id', controlledId);
    incognitoCheckbox.setAttribute('type', 'checkbox');
    incognitoCheckbox.checked = configs.incognitoAllowedExternalAddons.includes(addon.id);
    incognitoCheckbox.addEventListener('change', () => {
      const updatedValue = new Set(configs.incognitoAllowedExternalAddons);
      if (incognitoCheckbox.checked)
        updatedValue.add(addon.id);
      else
        updatedValue.delete(addon.id);
      configs.incognitoAllowedExternalAddons = Array.from(updatedValue);
      browser.runtime.sendMessage({
        type: TSTAPI.kCOMMAND_NOTIFY_PERMISSION_CHANGED,
        id:   addon.id
      });
    });

    const permissionsCell = row.appendChild(document.createElement('td'));
    if (addon.permissions.length > 0) {
      const permissionsLabel = permissionsCell.appendChild(document.createElement('label'));
      const permissionsCheckbox = permissionsLabel.appendChild(document.createElement('input'));
      permissionsCheckbox.setAttribute('id', controlledId);
      permissionsCheckbox.setAttribute('type', 'checkbox');
      permissionsCheckbox.checked = addon.permissionsGranted;
      permissionsCheckbox.addEventListener('change', () => {
        browser.runtime.sendMessage({
          type:        TSTAPI.kCOMMAND_SET_API_PERMISSION,
          id:          addon.id,
          permissions: permissionsCheckbox.checked ? addon.permissions : addon.permissions.map(permission => `!  ${permission}`)
        });
      });
      const permissionNames = addon.permissions.map(permission => {
        try {
          return browser.i18n.getMessage(`api_requestedPermissions_type_${permission}`) || permission;
        }
        catch(_error) {
          return permission;
        }
      }).join(', ');
      permissionsLabel.appendChild(document.createTextNode(permissionNames));
    }

    container.appendChild(row);
  }
}

function initSync() {
  const deviceInfoNameField = document.querySelector('#syncDeviceInfoName');
  deviceInfoNameField.addEventListener('input', () => {
    if (deviceInfoNameField.$throttling)
      clearTimeout(deviceInfoNameField.$throttling);
    deviceInfoNameField.$throttling = setTimeout(async () => {
      delete deviceInfoNameField.$throttling;
      configs.syncDeviceInfo = JSON.parse(JSON.stringify({
        ...(configs.syncDeviceInfo || await Sync.generateDeviceInfo()),
        name: deviceInfoNameField.value
      }));
    }, 250);
  });

  const deviceInfoIconRadiogroup = document.querySelector('#syncDeviceInfoIcon');
  deviceInfoIconRadiogroup.addEventListener('change', _event => {
    if (deviceInfoIconRadiogroup.$throttling)
      clearTimeout(deviceInfoIconRadiogroup.$throttling);
    deviceInfoIconRadiogroup.$throttling = setTimeout(async () => {
      delete deviceInfoIconRadiogroup.$throttling;
      const checkedRadio = deviceInfoIconRadiogroup.querySelector('input[type="radio"]:checked');
      configs.syncDeviceInfo = JSON.parse(JSON.stringify({
        ...(configs.syncDeviceInfo || await Sync.generateDeviceInfo()),
        icon: checkedRadio.value
      }));
    }, 250);
  });

  initOtherDevices();

  const otherDevices = document.querySelector('#otherDevices');
  otherDevices.addEventListener('click', event => {
    if (event.target.localName != 'button')
      return;
    const item = event.target.closest('li');
    removeOtherDevice(item.id.replace(/^otherDevice:/, ''));
  });
  otherDevices.addEventListener('keydown', event => {
    if (event.key != 'Enter' ||
        event.target.localName != 'button')
      return;
    const item = event.target.closest('li');
    removeOtherDevice(item.id.replace(/^otherDevice:/, ''));
  });
}

import('/extlib/codemirror.js').then(async () => {
  await Promise.all([
    configs.$loaded,
    import('/extlib/codemirror-mode-css.js'),
    import('/extlib/codemirror-colorpicker.js')
  ]);
  mUserStyleRulesFieldEditor = CodeMirror(mUserStyleRulesField, { // eslint-disable-line no-undef
    colorpicker: {
      mode: 'edit'
    },
    lineNumbers: true,
    lineWrapping: true,
    mode: 'css',
    theme: getUserStyleRulesFieldTheme()
  });
  mDarkModeMedia.addListener(async _event => {
    applyUserStyleRulesFieldTheme();
  });
  applyUserStyleRulesFieldTheme();
  window.mUserStyleRulesFieldEditor = mUserStyleRulesFieldEditor;
  mUserStyleRulesFieldEditor.setValue(loadUserStyleRules());
  mUserStyleRulesFieldEditor.on('change', reserveToSaveUserStyleRules);
  mUserStyleRulesFieldEditor.on('update', reserveToSaveUserStyleRules);
  initUserStyleImportExportButtons();
  initFileDragAndDropHandlers();
  mUserStyleRulesField.style.height = configs.userStyleRulesFieldHeight;
  (new ResizeObserver(_entries => {
    configs.userStyleRulesFieldHeight = `${mUserStyleRulesField.getBoundingClientRect().height}px`;
  })).observe(mUserStyleRulesField);
});
