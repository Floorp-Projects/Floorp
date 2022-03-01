/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import {
  log as internalLogger,
  configs,
  shouldApplyAnimation,
  notify,
  wait,
  sha1sum,
  sanitizeForHTMLText,
  isLinux,
} from './common.js';
import * as Permissions from './permissions.js';
import * as ApiTabs from './api-tabs.js';
import * as Constants from './constants.js';
import * as UserOperationBlocker from './user-operation-blocker.js';
import * as Dialog from './dialog.js';
import Tab from '/common/Tab.js';

import MenuUI from '/extlib/MenuUI.js';
import RichConfirm from '/extlib/RichConfirm.js';

function log(...args) {
  internalLogger('common/bookmarks', ...args);
}

let mCreatingCount = 0;

export async function getItemById(id) {
  if (!id)
    return null;
  try {
    const items = await browser.bookmarks.get(id).catch(ApiTabs.createErrorHandler());
    if (items.length > 0)
      return items[0];
  }
  catch(_error) {
  }
  return null;
}

function getAnimationDuration() {
  return shouldApplyAnimation() ? configs.collapseDuration : 0.001;
}

if (/^moz-extension:\/\/[^\/]+\/background\//.test(location.href)) {
  browser.runtime.onMessage.addListener((message, _sender) => {
    if (!message ||
        typeof message != 'object')
      return;

    switch (message.type) {
      case 'treestyletab:get-bookmark-item-by-id':
        return getItemById(message.id);

      case 'treestyletab:get-bookmark-child-items':
        return browser.bookmarks.getChildren(message.id || 'root________').catch(ApiTabs.createErrorHandler());

      case 'treestyletab:get-bookmark-ancestor-ids':
        return (async () => {
          const ancestorIds = [];
          let item;
          let lastId = message.id;
          do {
            item = await getItemById(lastId);
            if (!item)
              break;
            ancestorIds.push(lastId = item.parentId);
          } while (lastId != 'root________');
          return ancestorIds;
        })();
    }
  });
}

export async function bookmarkTab(tab, options = {}) {
  try {
    if (!(await Permissions.isGranted(Permissions.BOOKMARKS)))
      throw new Error('not permitted');
  }
  catch(_e) {
    notify({
      title:   browser.i18n.getMessage('bookmark_notification_notPermitted_title'),
      message: browser.i18n.getMessage(`bookmark_notification_notPermitted_message${isLinux() ? '_linux' : ''}`),
      url:     `moz-extension://${location.host}/options/options.html#bookmarksPermissionSection`
    });
    return null;
  }
  const parent = (
    (await getItemById(options.parentId || configs.defaultBookmarkParentId)) ||
    (await getItemById(configs.$defaults.defaultBookmarkParentId))
  );

  let title    = tab.title;
  let url      = tab.url;
  let parentId = parent && parent.id;
  if (options.showDialog) {
    const windowId = tab.windowId;
    const inSidebar = location.pathname.startsWith('/sidebar/');
    const divStyle = `
      display: flex;
      flex-direction: column;
    `;
    const labelStyle = `
      display: flex;
      flex-direction: ${inSidebar ? 'column' : 'row'};
      ${inSidebar ? 'align-items: stretch;' : ''}
      ${inSidebar ? 'text-align: start;' : ''}
    `;
    const labelTextStyle = `
      white-space: nowrap;
    `;
    const inputFieldStyle = `
      display: flex;
      ${inSidebar ? '' : 'margin-left: 0.25em;'}
      ${inSidebar ? '' : 'flex-grow: 1;'}
      ${inSidebar ? '' : 'flex-shrink: 1;'}
      ${inSidebar ? '' : 'min-width: 30em;'}
    `;
    const buttonContainerStyle = `
      ${inSidebar ? '' : 'margin-left: 0.25em;'}
    `;
    const dialogParams = {
      content: `
        <div style="${divStyle}"
            ><label accesskey=${JSON.stringify(sanitizeForHTMLText(browser.i18n.getMessage('bookmarkDialog_title_accessKey')))}
                    style="${labelStyle}"
                   ><span style="${labelTextStyle}"
                         >${sanitizeForHTMLText(browser.i18n.getMessage('bookmarkDialog_title'))}</span
                   ><input type="text"
                           name="title"
                           style="${inputFieldStyle}"
                           value=${JSON.stringify(sanitizeForHTMLText(title))}></label></div
       ><div style="${divStyle}"
            ><label accesskey=${JSON.stringify(sanitizeForHTMLText(browser.i18n.getMessage('bookmarkDialog_url_accessKey')))}
                    style="${labelStyle}"
                   ><span style="${labelTextStyle}"
                         >${sanitizeForHTMLText(browser.i18n.getMessage('bookmarkDialog_url'))}</span
                   ><input type="text"
                           name="url"
                           style="${inputFieldStyle}"
                           value=${JSON.stringify(sanitizeForHTMLText(url))}></label></div
       ><div style="${divStyle}; margin-bottom: 3em;"
            ><label style="${labelStyle}"
                   ><span style="${labelTextStyle}"
                         >${sanitizeForHTMLText(browser.i18n.getMessage('bookmarkDialog_parentId'))}</span
                   ><span style="${buttonContainerStyle}"
                         ><button name="parentId">-</button></span></label></div>
      `.trim(),
      async onShown(container, { MenuUI, initFolderChooser, animationDuration, parentId }) {
        if (container.classList.contains('simulation'))
          return;
        MenuUI.init();
        container.classList.add('bookmark-dialog');
        const [defaultItem, rootItems] = await Promise.all([
          browser.runtime.sendMessage({ type: 'treestyletab:get-bookmark-item-by-id', id: parentId }),
          browser.runtime.sendMessage({ type: 'treestyletab:get-bookmark-child-items' })
        ]);
        initFolderChooser(container.querySelector('button'), {
          MenuUI,
          animationDuration,
          defaultItem,
          rootItems
        });
        container.querySelector('[name="title"]').select();
      },
      inject: {
        MenuUI,
        initFolderChooser,
        animationDuration: getAnimationDuration(),
        parentId
      },
      buttons: [
        browser.i18n.getMessage('bookmarkDialog_accept'),
        browser.i18n.getMessage('bookmarkDialog_cancel')
      ]
    };
    let result;
    if (inSidebar) {
      try {
        UserOperationBlocker.blockIn(windowId, { throbber: false });
        result = await RichConfirm.show(dialogParams);
      }
      catch(_error) {
        result = { buttonIndex: -1 };
      }
      finally {
        UserOperationBlocker.unblockIn(windowId, { throbber: false });
      }
    }
    else {
      result = await Dialog.show(await browser.windows.get(windowId), {
        ...dialogParams,
        modal: true,
        type:  'dialog',
        url:   '/resources/blank.html', // required on Firefox ESR68
        title: browser.i18n.getMessage('bookmarkDialog_dialogTitle_single')
      });
    }
    if (result.buttonIndex != 0)
      return null;
    title    = result.values.title;
    url      = result.values.url;
    parentId = result.values.parentId;
  }

  mCreatingCount++;
  const item = await browser.bookmarks.create({
    parentId, title, url
  }).catch(ApiTabs.createErrorHandler());
  wait(150).then(() => {
    mCreatingCount--;
  });
  return item;
}

export async function bookmarkTabs(tabs, options = {}) {
  try {
    if (!(await Permissions.isGranted(Permissions.BOOKMARKS)))
      throw new Error('not permitted');
  }
  catch(_e) {
    notify({
      title:   browser.i18n.getMessage('bookmark_notification_notPermitted_title'),
      message: browser.i18n.getMessage('bookmark_notification_notPermitted_message'),
      url:     `moz-extension://${location.host}/options/options.html#bookmarksPermissionSection`
    });
    return null;
  }
  const now = new Date();
  const year = String(now.getFullYear());
  const title = configs.bookmarkTreeFolderName
    .replace(/%TITLE%/gi, tabs[0].title)
    .replace(/%URL%/gi, tabs[0].url)
    .replace(/%SHORT_?YEAR%/gi, year.substr(-2))
    .replace(/%(FULL_?)?YEAR%/gi, year)
    .replace(/%MONTH%/gi, String(now.getMonth() + 1).padStart(2, '0'))
    .replace(/%DATE%/gi, String(now.getDate()).padStart(2, '0'));
  const folderParams = {
    type: 'folder',
    title
  };
  let parent;
  if (options.parentId) {
    parent = await getItemById(options.parentId);
    if ('index' in options)
      folderParams.index = options.index;
  }
  else {
    parent = await getItemById(configs.defaultBookmarkParentId);
  }
  if (!parent)
    parent = await getItemById(configs.$defaults.defaultBookmarkParentId);
  if (parent)
    folderParams.parentId = parent.id;

  if (options.showDialog) {
    const windowId = tabs[0].windowId;
    const inSidebar = location.pathname.startsWith('/sidebar/');
    const divStyle = `
      display: flex;
      flex-direction: column;
    `;
    const labelStyle = `
      display: flex;
      flex-direction: ${inSidebar ? 'column' : 'row'};
      ${inSidebar ? 'align-items: stretch;' : ''}
      ${inSidebar ? 'text-align: start;' : ''}
    `;
    const labelTextStyle = `
      white-space: nowrap;
    `;
    const inputFieldStyle = `
      display: flex;
      ${inSidebar ? '' : 'margin-left: 0.25em;'}
      ${inSidebar ? '' : 'flex-grow: 1;'}
      ${inSidebar ? '' : 'flex-shrink: 1;'}
      ${inSidebar ? '' : 'min-width: 30em;'}
    `;
    const buttonContainerStyle = `
      ${inSidebar ? '' : 'margin-left: 0.25em;'}
    `;
    const dialogParams = {
      content: `
        <div style="${divStyle}"
            ><label accesskey=${JSON.stringify(sanitizeForHTMLText(browser.i18n.getMessage('bookmarkDialog_title_accessKey')))}
                    style="${labelStyle}"
                   ><span style="${labelTextStyle}"
                         >${sanitizeForHTMLText(browser.i18n.getMessage('bookmarkDialog_title'))}</span
                   ><input type="text"
                           name="title"
                           style="${inputFieldStyle}"
                           value=${JSON.stringify(sanitizeForHTMLText(folderParams.title))}></label></div
       ><div style="${divStyle}; margin-bottom: 3em;"
            ><label style="${labelStyle}"
                   ><span style="${labelTextStyle}"
                         >${sanitizeForHTMLText(browser.i18n.getMessage('bookmarkDialog_parentId'))}</span
                   ><span style="${buttonContainerStyle}"
                         ><button name="parentId">-</button></span></label></div>
      `.trim(),
      async onShown(container, { MenuUI, initFolderChooser, animationDuration, parentId }) {
        if (container.classList.contains('simulation'))
          return;
        MenuUI.init();
        container.classList.add('bookmark-dialog');
        const [defaultItem, rootItems] = await Promise.all([
          browser.runtime.sendMessage({ type: 'treestyletab:get-bookmark-item-by-id', id: parentId }),
          browser.runtime.sendMessage({ type: 'treestyletab:get-bookmark-child-items' })
        ]);
        initFolderChooser(container.querySelector('button'), {
          MenuUI,
          animationDuration,
          defaultItem,
          rootItems
        });
        container.querySelector('[name="title"]').select();
      },
      inject: {
        MenuUI,
        initFolderChooser,
        animationDuration: getAnimationDuration(),
        parentId: folderParams.parentId
      },
      buttons: [
        browser.i18n.getMessage('bookmarkDialog_accept'),
        browser.i18n.getMessage('bookmarkDialog_cancel')
      ]
    };
    let result;
    if (inSidebar) {
      try {
        UserOperationBlocker.blockIn(windowId, { throbber: false });
        result = await RichConfirm.show(dialogParams);
      }
      catch(_error) {
        result = { buttonIndex: -1 };
      }
      finally {
        UserOperationBlocker.unblockIn(windowId, { throbber: false });
      }
    }
    else {
      result = await Dialog.show(await browser.windows.get(windowId), {
        ...dialogParams,
        modal: true,
        type:  'dialog',
        url:   '/resources/blank.html', // required on Firefox ESR68
        title: browser.i18n.getMessage('bookmarkDialog_dialogTitle_multiple')
      });
    }
    if (result.buttonIndex != 0)
      return null;
    folderParams.title    = result.values.title;
    folderParams.parentId = result.values.parentId;
  }

  const toBeCreatedCount = tabs.length + 1;
  mCreatingCount += toBeCreatedCount;

  const titles = getTitlesWithTreeStructure(tabs);
  const folder = await browser.bookmarks.create(folderParams).catch(ApiTabs.createErrorHandler());
  for (let i = 0, maxi = tabs.length; i < maxi; i++) {
    await browser.bookmarks.create({
      parentId: folder.id,
      index:    i,
      title:    titles[i],
      url:      tabs[i].url
    }).catch(ApiTabs.createErrorSuppressor());
  }

  wait(150).then(() => {
    mCreatingCount -= toBeCreatedCount;
  });

  return folder;
}

function getTitlesWithTreeStructure(tabs) {
  const minLevel = Math.min(...tabs.map(tab => parseInt(tab.$TST.getAttribute(Constants.kLEVEL) || '0')));
  const titles = [];
  for (let i = 0, maxi = tabs.length; i < maxi; i++) {
    const tab = tabs[i];
    const title = tab.title;
    const level = parseInt(tab.$TST.getAttribute(Constants.kLEVEL) || '0') - minLevel;
    let prefix = '';
    for (let j = 0; j < level; j++) {
      prefix += '>';
    }
    if (prefix)
      titles.push(`${prefix} ${title}`);
    else
      titles.push(title.replace(/^>+ /, '')); // if the page title has marker-like prefix, we need to remove it.
  }
  return titles;
}

export async function initFolderChooser(anchor, params = {}) {
  let chooserTree = window.$bookmarkFolderChooserTree;
  if (!chooserTree) {
    chooserTree = window.$bookmarkFolderChooserTree = document.documentElement.appendChild(document.createElement('ul'));
  }
  else {
    const range = document.createRange();
    range.selectNodeContents(chooserTree);
    range.deleteContents();
    range.detach();
  }

  delete anchor.dataset.value;
  anchor.textContent = browser.i18n.getMessage('bookmarkFolderChooser_unspecified');

  anchor.style.overflow     = 'hidden';
  anchor.style.textOverflow = 'ellipsis';
  anchor.style.whiteSpace   = 'pre';

  let lastChosenId = null;
  if (params.defaultItem || params.defaultValue) {
    const item = params.defaultItem || await getItemById(params.defaultValue);
    if (item) {
      lastChosenId         = item.id;
      anchor.dataset.value = lastChosenId;
      anchor.dataset.title = item.title;
      anchor.textContent   = item.title || browser.i18n.getMessage('bookmarkFolderChooser_blank');
      anchor.setAttribute('title', anchor.textContent);
    }
  }

  // eslint-disable-next-line prefer-const
  let topLevelItems;
  anchor.ui = new (params.MenuUI || MenuUI)({
    root:       chooserTree,
    appearance: 'menu',
    onCommand(item, event) {
      if (item.dataset.id) {
        lastChosenId         = item.dataset.id;
        anchor.dataset.value = lastChosenId;
        anchor.dataset.title = item.dataset.title;
        anchor.textContent   = item.dataset.title || browser.i18n.getMessage('bookmarkFolderChooser_blank');
        anchor.setAttribute('title', anchor.textContent);
      }
      if (typeof params.onCommand == 'function')
        params.onCommand(item, event);
      anchor.ui.close();
    },
    onShown() {
      for (const item of chooserTree.querySelectorAll('.checked')) {
        item.classList.remove('checked');
      }
      if (lastChosenId) {
        const item = chooserTree.querySelector(`.radio[data-id="${lastChosenId}"]`);
        if (item)
          item.classList.add('checked');
      }
    },
    onHidden() {
      const range = document.createRange();
      for (const folderItem of topLevelItems) {
        const separator = folderItem.lastChild.querySelector('.separator');
        if (!separator)
          continue;
        range.selectNodeContents(folderItem.lastChild);
        range.setStartBefore(separator);
        range.deleteContents();
        folderItem.classList.remove('has-built-children');
      }
      range.detach();
    },
    animationDuration: params.animationDuration || getAnimationDuration()
  });
  anchor.addEventListener('click', () => {
    anchor.ui.open({
      anchor
    });
  });
  anchor.addEventListener('keydown', event => {
    if (event.key == 'Enter')
      anchor.ui.open({
        anchor
      });
  });

  const generateFolderItem = (folder) => {
    const item = document.createElement('li');
    item.appendChild(document.createTextNode(folder.title));
    item.setAttribute('title', folder.title || browser.i18n.getMessage('bookmarkFolderChooser_blank'));
    item.dataset.id    = folder.id;
    item.dataset.title = folder.title;
    item.classList.add('folder');
    item.classList.add('radio');
    const container = item.appendChild(document.createElement('ul'));
    const useThisItem = container.appendChild(document.createElement('li'));
    useThisItem.textContent   = browser.i18n.getMessage('bookmarkFolderChooser_useThisFolder');
    useThisItem.dataset.id    = folder.id;
    useThisItem.dataset.title = folder.title;
    return item;
  };

  const buildItems = async (items, container) => {
    const createdItems = [];
    for (const item of items) {
      if (item.type == 'bookmark' &&
          /^place:parent=([^&]+)$/.test(item.url)) { // alias for special folders
        const realItem = await browser.runtime.sendMessage({
          type: 'treestyletab:get-bookmark-item-by-id',
          id:   RegExp.$1
        });
        item.id    = realItem.id;
        item.type  = realItem.type;
        item.title = realItem.title;
      }
      if (item.type != 'folder')
        continue;
      const folderItem = generateFolderItem(item);
      container.appendChild(folderItem);
      createdItems.push(folderItem);
      folderItem.$completeFolderItem = async () => {
        if (!item.$fetched &&
            !('children' in item)) {
          item.$fetched = true;
          item.children = await browser.runtime.sendMessage({
            type: 'treestyletab:get-bookmark-child-items',
            id:   item.id
          });
        }
        if (item.children &&
            item.children.length > 0 &&
            !folderItem.classList.contains('has-built-children')) {
          folderItem.classList.add('has-built-children');
          await buildItems(item.children, folderItem.lastChild);
          anchor.ui.updateMenuItem(folderItem);
        }
        return folderItem;
      };
      folderItem.addEventListener('focus', folderItem.$completeFolderItem);
      folderItem.addEventListener('mouseover', folderItem.$completeFolderItem);
    }
    const firstFolderItem = container.querySelector('.folder');
    if (firstFolderItem && firstFolderItem.previousSibling) {
      const separator = container.insertBefore(document.createElement('li'), firstFolderItem);
      separator.classList.add('separator');
    }
    return createdItems;
  };

  topLevelItems = await buildItems(params.rootItems, chooserTree);
}

let mCreatedBookmarks = [];

async function onBookmarksCreated(id, bookmark) {
  log('onBookmarksCreated ', { id, bookmark });

  if (mCreatingCount > 0)
    return;

  mCreatedBookmarks.push(bookmark);
  reserveToGroupCreatedBookmarks();
}

function reserveToGroupCreatedBookmarks() {
  if (reserveToGroupCreatedBookmarks.reserved)
    clearTimeout(reserveToGroupCreatedBookmarks.reserved);
  reserveToGroupCreatedBookmarks.reserved = setTimeout(() => {
    reserveToGroupCreatedBookmarks.reserved = null;
    tryGroupCreatedBookmarks();
  }, 250);
}
reserveToGroupCreatedBookmarks.reserved = null;
reserveToGroupCreatedBookmarks.retryCount = 0;

async function tryGroupCreatedBookmarks() {
  log('tryGroupCreatedBookmarks ', mCreatedBookmarks);

  const lastDraggedTabs = configs.lastDraggedTabs;
  if (lastDraggedTabs &&
      lastDraggedTabs.tabIds.length > mCreatedBookmarks.length) {
    if (reserveToGroupCreatedBookmarks.retryCount++ < 10) {
      return reserveToGroupCreatedBookmarks();
    }
    else {
      reserveToGroupCreatedBookmarks.retryCount = 0;
      mCreatedBookmarks = [];
      configs.lastDraggedTabs = null;
      log(' => timeout');
      return;
    }
  }
  reserveToGroupCreatedBookmarks.retryCount = 0;

  const bookmarks = mCreatedBookmarks;
  mCreatedBookmarks = [];
  {
    // accept only bookmarks from dragged tabs
    const digest = await sha1sum(bookmarks.map(tab => tab.url).join('\n'));
    configs.lastDraggedTabs = null;
    if (digest != lastDraggedTabs.urlsDigest) {
      log(' => digest mismatched ', { digest, last: lastDraggedTabs.urlsDigest });
      return;
    }
  }

  if (bookmarks.length < 2) {
    log(' => ignore single bookmark');
    return;
  }

  {
    // Do nothing if multiple bookmarks are created under
    // multiple parent folders by sync.
    const parentIds = new Set();
    for (const bookmark of bookmarks) {
      parentIds.add(bookmark.parentId);
    }
    if (parentIds.size > 1) {
      log(' => ignore bookmarks created under multiple folders');
      return;
    }
  }

  const parentId = bookmarks[0].parentId;
  {
    // Do nothing if all bookmarks are created under a new
    // blank folder.
    const allChildren = await browser.bookmarks.getChildren(parentId);
    if (allChildren.length == bookmarks.length) {
      log(' => ignore bookmarks created under a new blank folder');
      return;
    }
  }

  log('ready to group bookmarks under a folder');

  log('create a folder for grouping');
  mCreatingCount++;
  const folder = await browser.bookmarks.create({
    type:  'folder',
    title: bookmarks[0].title,
    index: bookmarks[0].index,
    parentId
  }).catch(ApiTabs.createErrorHandler());
  wait(150).then(() => {
    mCreatingCount--;
  });

  log('move into a folder');
  let movedCount = 0;
  for (const bookmark of bookmarks) {
    await browser.bookmarks.move(bookmark.id, {
      parentId: folder.id,
      index:    movedCount++
    });
  }

  const tabs = lastDraggedTabs.tabIds.map(id => Tab.get(id));
  let titles = getTitlesWithTreeStructure(tabs);
  if (tabs[0].$TST.isGroupTab &&
      titles.filter(title => !/^>/.test(title)).length == 1) {
    log('delete needless bookmark for a group tab');
    browser.bookmarks.remove(bookmarks[0].id);
    tabs.shift();
    bookmarks.shift();
    titles = getTitlesWithTreeStructure(tabs);
  }

  log('save tree structure to bookmarks');
  for (let i = 0, maxi = bookmarks.length; i < maxi; i++) {
    const title = titles[i];
    if (title == tabs[i].title)
      continue;
    browser.bookmarks.update(bookmarks[i].id, { title });
  }
}

export async function startTracking() {
  const granted = await Permissions.isGranted(Permissions.BOOKMARKS);
  if (granted && !browser.bookmarks.onCreated.hasListener(onBookmarksCreated))
    browser.bookmarks.onCreated.addListener(onBookmarksCreated);
}
