/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

(function prepare(retryCount = 0) {
  if (retryCount > 10)
    throw new Error('could not prepare group tab contents');

  if (!document.documentElement) {
    setTimeout(prepare, 100, retryCount + 1);
    return false;
  }

  if (window.prepared ||
      document.documentElement.classList.contains('initialized'))
    return false;

  window.prepared = true;

  let gTitle;
  let gTitleField;
  let gTemporaryCheck;
  let gTemporaryAggressiveCheck;
  let gBrowserThemeDefinition;
  let gUserStyleRules;

  const url = new URL(location.href);

  document.title = getTitle();

  function getTitle() {
    let title = url.searchParams.get('title');
    if (!title) {
      const matched = location.search.match(/^\?([^&;]*)/);
      if (matched)
        title = decodeURIComponent(matched[1]);
    }
    return title || browser.i18n.getMessage('groupTab_label_default');
  }

  function setTitle(title) {
    if (!gTitle)
      init();
    document.title = gTitle.textContent = gTitleField.value = title;
    updateParameters({ title });
  }

  function isTemporary() {
    return url.searchParams.get('temporary') == 'true';
  }

  function isTemporaryAggressive() {
    return url.searchParams.get('temporaryAggressive') == 'true';
  }

  function getOpenerTabId() {
    return url.searchParams.get('openerTabId');
  }

  function enterTitleEdit() {
    if (!gTitle)
      init();
    gTitle.style.display = 'none';
    gTitleField.style.display = 'inline';
    gTitleField.select();
    gTitleField.focus();
  }

  function exitTitleEdit() {
    if (!gTitle)
      init();
    gTitle.style.display = '';
    gTitleField.style.display = '';
  }

  function hasModifier(event) {
    return event.altKey ||
           event.ctrlKey ||
           event.metaKey ||
           event.shiftKey;
  }

  function updateParameters({ title } = {}) {
    url.searchParams.set('title', title || getTitle() || '');

    if (gTemporaryCheck.checked)
      url.searchParams.set('temporary', 'true');
    else
      url.searchParams.delete('temporary');

    if (gTemporaryAggressiveCheck.checked)
      url.searchParams.set('temporaryAggressive', 'true');
    else
      url.searchParams.delete('temporaryAggressive');

    const opener = getOpenerTabId();
    if (opener)
      url.searchParams.set('openerTabId', opener);
    else
      url.searchParams.delete('openerTabId');

    history.replaceState({}, document.title, url.href);
  }

  async function init(retryCount = 0) {
    if (gTitle &&
        gTitleField &&
        gTemporaryCheck &&
        gTemporaryAggressiveCheck &&
        gBrowserThemeDefinition &&
        gUserStyleRules)
      return;

    if (retryCount > 10)
      throw new Error('could not initialize group tab contents');

    gTitle                    = document.querySelector('#title');
    gTitleField               = document.querySelector('#title-field');
    gTemporaryCheck           = document.querySelector('#temporary');
    gTemporaryAggressiveCheck = document.querySelector('#temporaryAggressive');
    gBrowserThemeDefinition   = document.querySelector('#browser-theme-definition');
    gUserStyleRules           = document.querySelector('#user-style-rules');

    if (!gTitle ||
        !gTitleField ||
        !gTemporaryCheck ||
        !gTemporaryAggressiveCheck ||
        !gBrowserThemeDefinition ||
        !gUserStyleRules) {
      setTimeout(init, 100, retryCount + 1);
      return;
    }

    gTitle.addEventListener('click', event => {
      if (event.button == 0 &&
          !hasModifier(event)) {
        enterTitleEdit();
        event.stopPropagation();
      }
    });
    gTitleField.addEventListener('keydown', event => {
      // Event.isComposing for the Enter key to finish composition is always
      // "false" on keyup, so we need to handle this on keydown.
      if (hasModifier(event) ||
          event.isComposing)
        return;

      switch (event.key) {
        case 'Escape':
          gTitleField.value = gTitle.textContent;
          exitTitleEdit();
          break;

        case 'Enter':
          setTitle(gTitleField.value);
          exitTitleEdit();
          break;

        case 'F2':
          event.stopPropagation();
          break;
      }
    });
    window.addEventListener('click', event => {
      const link = event.target.closest('a');
      if (link) {
        browser.runtime.sendMessage({
          type: 'treestyletab:api:focus',
          tab:  parseInt(link.dataset.tabId)
        });
        return;
      }
      if (event.button != 0 ||
          hasModifier(event))
        return;
      if (event.target != gTitleField) {
        setTitle(gTitleField.value);
        exitTitleEdit();
        event.stopPropagation();
      }
    });
    window.addEventListener('keyup', event => {
      if (event.key == 'F2' &&
          !hasModifier(event))
        enterTitleEdit();
    });

    window.addEventListener('resize', reflow);

    gTitle.textContent = gTitleField.value = getTitle();

    gTemporaryCheck.checked = isTemporary();
    gTemporaryCheck.addEventListener('change', _event => {
      if (gTemporaryCheck.checked)
        gTemporaryAggressiveCheck.checked = false;
      updateParameters();
    });

    gTemporaryAggressiveCheck.checked = isTemporaryAggressive();
    gTemporaryAggressiveCheck.addEventListener('change', _event => {
      if (gTemporaryAggressiveCheck.checked)
        gTemporaryCheck.checked = false;
      updateParameters();
    });

    window.setTitle    = window.setTitle || setTitle;
    window.updateTree  = window.updateTree || updateTree;

    window.l10n.updateDocument();

    const [themeDeclarations, contextualIdentitiesColorInfo, configs, userStyleRules] = await Promise.all([
      browser.runtime.sendMessage({
        type: 'treestyletab:get-theme-declarations'
      }),
      browser.runtime.sendMessage({
        type: 'treestyletab:get-contextual-identities-color-info'
      }),
      browser.runtime.sendMessage({
        type: 'treestyletab:get-config-value',
        keys: [
          'renderTreeInGroupTabs',
          'showAutoGroupOptionHint'
        ]
      }),
      browser.runtime.sendMessage({
        type: 'treestyletab:get-user-style-rules'
      })
    ]);

    const contextualIdentitiesMarkerDeclarations = Object.keys(contextualIdentitiesColorInfo.colors).map(id =>
      `#tabs a[data-cookie-store-id="${id}"] .contextual-identity-marker {
         background-color: ${contextualIdentitiesColorInfo.colors[id]};
       }`).join('\n');
    gBrowserThemeDefinition.textContent = `
      ${themeDeclarations}
      ${contextualIdentitiesMarkerDeclarations}
      ${contextualIdentitiesColorInfo.colorDeclarations}
    `;
    gUserStyleRules.textContent = userStyleRules;

    updateTree.enabled = configs.renderTreeInGroupTabs;
    updateTree();

    let show = configs.showAutoGroupOptionHint;
    if (!isTemporary() && !isTemporaryAggressive())
      show = false;

    const hint = document.getElementById('optionHint');
    hint.style.display = show ? 'block' : 'none';

    if (show) {
      hint.firstChild.addEventListener('click', event => {
        if (event.button != 0)
          return;
        browser.runtime.sendMessage({
          type: 'treestyletab:open-tab',
          uri:  `moz-extension://${location.host}/options/options.html#autoGroupNewTabsSection`
        });
      });
      hint.firstChild.addEventListener('keydown', event => {
        if (event.key != 'Enter' &&
            event.key != 'Space')
          return;
        browser.runtime.sendMessage({
          type: 'treestyletab:open-tab',
          uri:  `moz-extension://${location.host}/options/options.html#autoGroupNewTabsSection`
        });
      });

      const closebox = document.getElementById('dismissOptionHint');
      closebox.addEventListener('click', event => {
        if (event.button != 0)
          return;
        hint.style.display = 'none';
        browser.runtime.sendMessage({
          type: 'treestyletab:set-config-value',
          key:  'showAutoGroupOptionHint',
          value: false
        });
      });
      closebox.addEventListener('keydown', event => {
        if (event.key != 'Enter' &&
            event.key != 'Space')
          return;
        hint.style.display = 'none';
        browser.runtime.sendMessage({
          type: 'treestyletab:set-config-value',
          key:  'showAutoGroupOptionHint',
          value: false
        });
      });
    }

    document.documentElement.classList.add('initialized');
  }
  //document.addEventListener('DOMContentLoaded', init, { once: true });


  async function updateTree() {
    const runAt = updateTree.lastRun = Date.now();

    const container = document.getElementById('tabs');
    const range = document.createRange();
    range.selectNodeContents(container);
    range.deleteContents();
    range.detach();

    if (!updateTree.enabled) {
      document.documentElement.classList.remove('updating');
      return;
    }

    document.documentElement.classList.add('updating');

    const tabs = await browser.runtime.sendMessage({
      type: 'treestyletab:api:get-tree',
      tabs: [
        'senderTab',
        getOpenerTabId()
      ],
      interval: 50
    });

    // called again while waiting
    if (runAt != updateTree.lastRun)
      return;

    if (!tabs) {
      console.error(new Error('Couldn\'t get tree of tabs unexpectedly.'));
      document.documentElement.classList.remove('updating');
      return;
    }

    let tree;
    if (tabs[1]) {
      tabs[1].children = tabs[0].children;
      tree = buildChildren({ children: [tabs[1]] });
    }
    else
      tree = buildChildren(tabs[0]);
    if (tree) {
      container.appendChild(tree);
      reflow();
    }

    document.documentElement.classList.remove('updating');
  }
  updateTree.enabled = true;

  function reflow() {
    const container = document.getElementById('tabs');
    columnizeTree(container.firstChild, {
      columnWidth: 'var(--column-width, 20em)',
      containerRect: container.getBoundingClientRect()
    });
  }

  function buildItem(tab) {
    const item = document.createElement('li');

    const link = item.appendChild(document.createElement('a'));
    link.href = '#';
    link.setAttribute('title', tab.cookieStoreName ? `${tab.title} - ${tab.cookieStoreName}` : tab.title);
    link.dataset.tabId = tab.id;
    link.dataset.cookieStoreId = tab.cookieStoreId;

    const contextualIdentityMarker = link.appendChild(document.createElement('span'));
    contextualIdentityMarker.classList.add('contextual-identity-marker');

    const defaultFavIcon = link.appendChild(document.createElement('span'));
    defaultFavIcon.classList.add('default-favicon');

    const icon = link.appendChild(document.createElement('img'));
    if (tab.effectiveFavIconUrl || tab.favIconUrl) {
      icon.src = tab.effectiveFavIconUrl || tab.favIconUrl;
      icon.onerror = () => {
        item.classList.remove('favicon-loading');
        item.classList.add('use-default-favicon');
      };
      icon.onload = () => {
        item.classList.remove('favicon-loading');
      };
      item.classList.add('favicon-loading');
    }
    else {
      item.classList.add('use-default-favicon');
    }

    if (Array.isArray(tab.states) &&
        tab.states.includes('group-tab'))
      item.classList.add('group-tab');

    const label = link.appendChild(document.createElement('span'));
    label.classList.add('label');
    label.textContent = tab.title;

    const children = buildChildren(tab);
    if (!children)
      return item;

    const fragment = document.createDocumentFragment();
    fragment.appendChild(item);
    const childrenWrapped = document.createElement('li');
    childrenWrapped.appendChild(children);
    fragment.appendChild(childrenWrapped);
    return fragment;
  }

  function buildChildren(tab) {
    if (tab.children && tab.children.length > 0) {
      const list = document.createElement('ul');
      for (const child of tab.children) {
        list.appendChild(buildItem(child));
      }
      return list;
    }
    return null;
  }

  function columnizeTree(aTree, options) {
    options = options || {};
    options.columnWidth = options.columnWidth || 'var(--column-width, 20em)';

    const style = aTree.style;
    style.columnWidth = style.MozColumnWidth = `calc(${options.columnWidth})`;
    const computedStyle = window.getComputedStyle(aTree, null);
    aTree.columnWidth = Number((computedStyle.MozColumnWidth || computedStyle.columnWidth).replace(/px/, ''));
    style.columnGap   = style.MozColumnGap = '1em';
    style.columnFill  = style.MozColumnFill = 'auto';
    style.columnCount = style.MozColumnCount = 'auto';

    const containerRect = options.containerRect || aTree.parentNode.getBoundingClientRect();
    const maxWidth = containerRect.width;
    if (aTree.columnWidth * 2 <= maxWidth ||
        options.calculateCount) {
      const treeContentsRange = document.createRange();
      treeContentsRange.selectNodeContents(aTree);
      const overflow = treeContentsRange.getBoundingClientRect().width > window.innerWidth;
      treeContentsRange.detach();
      const blankSpace = overflow ? 2 : 1;
      style.height = style.maxHeight =
        `calc(${containerRect.height}px - ${blankSpace}em)`;

      if (getActualColumnCount(aTree) <= 1)
        style.columnWidth = style.MozColumnWidth = '';
    }
    else {
      style.height = style.maxHeight = '';
    }
  }

  function getActualColumnCount(aTree) {
    const range = document.createRange();
    range.selectNodeContents(aTree);
    const rect = range.getBoundingClientRect();
    range.detach();
    return Math.floor(rect.width / aTree.columnWidth);
  }

  init();
  return true;
})();
