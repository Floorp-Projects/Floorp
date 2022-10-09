/*
 license: The MIT License, Copyright (c) 2018-2021 YUKI "Piro" Hiroshi
 original:
   https://github.com/piroor/webextensions-lib-rich-confirm
*/
'use strict';

(function defineRichConfirm(uniqueKey) {
  class RichConfirm {
    constructor(params) {
      this.params = params;
      if (!this.params.buttons)
        this.params.buttons = ['OK'];
      this.onClick = this.onClick.bind(this);
      this.onKeyDown = this.onKeyDown.bind(this);
      this.onKeyUp = this.onKeyUp.bind(this);
      this.onContextMenu = this.onContextMenu.bind(this);
      this.onUnload = this.onUnload.bind(this);
    }
    get commonClass() {
      return `rich-confirm-${this.uniqueKey}`;
    }
    get dialog() {
      return this.ui.querySelector('.rich-confirm-dialog');
    }
    get content() {
      return this.ui.querySelector('.rich-confirm-content');
    }
    get buttonsContainer() {
      return this.ui.querySelector('.rich-confirm-buttons');
    }
    get checkContainer() {
      return this.ui.querySelector('.rich-confirm-check-label');
    }
    get checkCheckbox() {
      return this.ui.querySelector('.rich-confirm-check-checkbox');
    }
    get checkMessage() {
      return this.ui.querySelector('.rich-confirm-check-message');
    }

    get focusTargets() {
      return Array.from(this.ui.querySelectorAll('input:not([type="hidden"]), textarea, select, button')).filter(node => node.offsetWidth > 0);
    }

    buildUI() {
      if (this.ui)
        return;
      this.style = document.createElement('style');
      this.style.setAttribute('type', 'text/css');
      const common = `.${this.commonClass}`;
      this.style.textContent = `
        /* color scheme */
        ${common}.rich-confirm,
        :root${common} {
          /* https://hg.mozilla.org/mozilla-central/raw-file/tip/toolkit/themes/shared/in-content/common.inc.css */
          --in-content-page-color: var(--grey-90);
          --in-content-page-background: var(--grey-10);
          --in-content-text-color: var(--in-content-page-color);
          --in-content-deemphasized-text: var(--grey-60);
          --in-content-selected-text: #fff;
          --in-content-box-background: #fff;
          --in-content-box-background-hover: var(--grey-20);
          --in-content-box-background-active: var(--grey-30);
          --in-content-box-border-color: var(--grey-90-a30);
          --in-content-box-border-color-mixed: rgb(calc((249 * 0.7) + (12 * 0.3)), calc((249 * 0.7) + (12 * 0.3)), calc((250 * 0.7) + (13 * 0.3)));
          --in-content-box-info-background: var(--grey-20);
          --in-content-item-hover: rgba(69, 161, 255, 0.2); /* blue 40 a20 */
          --in-content-item-hover-mixed: rgb(calc((249 * 0.8) + (69 * 0.2)), calc((249 * 0.8) + (161 * 0.2)), calc((250 * 0.8) + (255 * 0.2)));
          --in-content-item-selected: var(--blue-50);
          --in-content-border-highlight: var(--blue-50);
          --in-content-border-focus: var(--blue-50);
          --in-content-border-hover: var(--grey-90-a50);
          --in-content-border-hover-mixed: rgb(calc((249 * 0.5) + (12 * 0.5)), calc((249 * 0.5) + (12 * 0.5)), calc((250 * 0.5) + (13 * 0.5)));
          --in-content-border-active: var(--blue-50);
          --in-content-border-active-shadow: var(--blue-50-a30);
          --in-content-border-invalid: var(--red-50);
          --in-content-border-invalid-shadow: var(--red-50-a30);
          --in-content-border-color: var(--grey-30);
          --in-content-border-color-mixed: #d7d7db;
          --in-content-category-outline-focus: 1px dotted var(--blue-50);
          --in-content-category-text-selected: var(--blue-50);
          --in-content-category-text-selected-active: var(--blue-60);
          --in-content-category-background-hover: rgba(12,12,13,0.1);
          --in-content-category-background-active: rgba(12,12,13,0.15);
          --in-content-category-background-selected-hover: rgba(12,12,13,0.15);
          --in-content-category-background-selected-active: rgba(12,12,13,0.2);
          --in-content-tab-color: #424f5a;
          --in-content-link-color: var(--blue-60);
          --in-content-link-color-hover: var(--blue-70);
          --in-content-link-color-active: var(--blue-80);
          --in-content-link-color-visited: var(--blue-60);
          --in-content-button-background: var(--grey-90-a10);
          --in-content-button-background-mixed: rgb(calc((249 * 0.9) + (12 * 0.1)), calc((249 * 0.9) + (12 * 0.1)), calc((250 * 0.9) + (13 * 0.1)));
          --in-content-button-background-hover: var(--grey-90-a20);
          --in-content-button-background-hover-mixed: rgb(calc((249 * 0.8) + (12 * 0.2)), calc((249 * 0.8) + (12 * 0.2)), calc((250 * 0.8) + (13 * 0.2)));
          --in-content-button-background-active: var(--grey-90-a30);
          --in-content-button-background-active-mixed: rgb(calc((249 * 0.7) + (12 * 0.3)), calc((249 * 0.7) + (12 * 0.3)), calc((250 * 0.7) + (13 * 0.3)));

          --blue-40: #45a1ff;
          --blue-40-a10: rgb(69, 161, 255, 0.1);
          --blue-50: #0a84ff;
          --blue-50-a30: rgba(10, 132, 255, 0.3);
          --blue-60: #0060df;
          --blue-70: #003eaa;
          --blue-80: #002275;
          --grey-10: #f9f9fa;
          --grey-10-a015: rgba(249, 249, 250, 0.015);
          --grey-10-a20: rgba(249, 249, 250, 0.2);
          --grey-20: #ededf0;
          --grey-30: #d7d7db;
          --grey-40: #b1b1b3;
          --grey-60: #4a4a4f;
          --grey-90: #0c0c0d;
          --grey-90-a10: rgba(12, 12, 13, 0.1);
          --grey-90-a20: rgba(12, 12, 13, 0.2);
          --grey-90-a30: rgba(12, 12, 13, 0.3);
          --grey-90-a50: rgba(12, 12, 13, 0.5);
          --grey-90-a60: rgba(12, 12, 13, 0.6);
          --green-50: #30e60b;
          --green-60: #12bc00;
          --green-70: #058b00;
          --green-80: #006504;
          --green-90: #003706;
          --orange-50: #ff9400;
          --purple-70: #6200a4;
          --red-50: #ff0039;
          --red-50-a30: rgba(255, 0, 57, 0.3);
          --red-60: #d70022;
          --red-70: #a4000f;
          --red-80: #5a0002;
          --red-90: #3e0200;
          --yellow-10: #ffff98;
          --yellow-50: #ffe900;
          --yellow-60: #d7b600;
          --yellow-60-a30: rgba(215, 182, 0, 0.3);
          --yellow-70: #a47f00;
          --yellow-80: #715100;
          --yellow-90: #3e2800;

          /* https://hg.mozilla.org/mozilla-central/raw-file/tip/browser/themes/addons/dark/manifest.json */
          --dark-frame: hsl(240, 5%, 5%);
          --dark-icons: rgb(249, 249, 250, 0.7);
          --dark-ntp-background: #2A2A2E;
          --dark-ntp-text: rgb(249, 249, 250);
          --dark-popup: #4a4a4f;
          --dark-popup-border: #27272b;
          --dark-popup-text: rgb(249, 249, 250);
          --dark-sidebar: #38383D;
          --dark-sidebar-text: rgb(249, 249, 250);
          --dark-sidebar-border: rgba(255, 255, 255, 0.1);
          --dark-tab-background-text: rgb(249, 249, 250);
          --dark-tab-line: #0a84ff;
          --dark-toolbar: hsl(240, 1%, 20%);
          --dark-toolbar-bottom-separator: hsl(240, 5%, 5%);
          --dark-toolbar-field: rgb(71, 71, 73);
          --dark-toolbar-field-border: rgba(249, 249, 250, 0.2);
          --dark-toolbar-field-separator: #5F6670;
          --dark-toolbar-field-text: rgb(249, 249, 250);

          /* https://searchfox.org/mozilla-central/rev/35873cfc312a6285f54aa5e4ec2d4ab911157522/browser/themes/shared/tabs.inc.css#24 */
          --tab-loading-fill: #0A84FF;


          --bg-color: var(--grey-10);
          --text-color: var(--grey-90);
        }

        ${common}.rich-confirm :link {
          color: var(--in-content-link-color);
        }
        ${common}.rich-confirm :visited {
          color: var(--in-content-link-color-visited);
        }

        ${common}.rich-confirm :link:hover,
        ${common}.rich-confirm :visited:hover {
          color: var(--in-content-link-color-hover);
        }

        ${common}.rich-confirm :link:active,
        ${common}.rich-confirm :visited:active {
          color: var(--in-content-link-color-active);
        }

        @media (prefers-color-scheme: dark) {
          ${common}.rich-confirm,
          :root${common} {
            /* https://hg.mozilla.org/mozilla-central/raw-file/tip/toolkit/themes/shared/in-content/common.inc.css */
            --in-content-page-background: #2A2A2E /* rgb(42, 42, 46) */;
            --in-content-page-color: rgb(249, 249, 250);
            --in-content-text-color: var(--in-content-page-color);
            --in-content-deemphasized-text: var(--grey-40);
            --in-content-box-background: #202023;
            --in-content-box-background-hover: /* rgba(249,249,250,0.15) */ rgb(calc((42 * 0.85) + (249 * 0.15)), calc((42 * 0.85) + (249 * 0.15)), calc((46 * 0.85) + (250 * 0.15)));
            --in-content-box-background-active: /*rgba(249,249,250,0.2) */ rgb(calc((42 * 0.8) + (249 * 0.2)), calc((42 * 0.8) + (249 * 0.2)), calc((46 * 0.8) + (250 * 0.2)));
            --in-content-box-background-odd: rgba(249,249,250,0.05);
            --in-content-box-info-background: rgba(249,249,250,0.15);

            --in-content-border-color: rgba(249,249,250,0.2);
            --in-content-border-color-mixed: rgb(calc((42 * 0.8) + (249 * 0.2)), calc((42 * 0.8) + (249 * 0.2)), calc((46 * 0.8) + (250 * 0.2)));
            --in-content-border-hover: rgba(249,249,250,0.3);
            --in-content-border-hover-mixed: rgb(calc((42 * 0.7) + (249 * 0.3)), calc((42 * 0.7) + (249 * 0.3)), calc((46 * 0.7) + (250 * 0.3)));
            --in-content-box-border-color: rgba(249,249,250,0.2);
            --in-content-box-border-color-mixed: rgb(calc((42 * 0.8) + (249 * 0.2)), calc((42 * 0.8) + (249 * 0.2)), calc((46 * 0.8) + (250 * 0.2)));

            --in-content-button-background: rgba(249,249,250,0.1);
            --in-content-button-background-mixed: rgb(calc((42 * 0.9) + (249 * 0.1)), calc((42 * 0.9) + (249 * 0.1)), calc((46 * 0.9) + (250 * 0.1)));
            --in-content-button-background-hover: rgba(249,249,250,0.15);
            --in-content-button-background-hover-mixed: rgb(calc((42 * 0.85) + (249 * 0.15)), calc((42 * 0.85) + (249 * 0.15)), calc((46 * 0.85) + (250 * 0.15)));
            --in-content-button-background-active: rgba(249,249,250,0.2);
            --in-content-button-background-active-mixed: rgb(calc((42 * 0.8) + (249 * 0.2)), calc((42 * 0.8) + (249 * 0.2)), calc((46 * 0.8) + (250 * 0.2)));

            --in-content-link-color: var(--blue-40);
            --in-content-link-color-hover: var(--blue-50);
            --in-content-link-color-active: var(--blue-60);

            --bg-color: var(--in-content-page-background);
            --text-color: var(--in-content-text-color);
          }

          ${common}.rich-confirm textarea,
          ${common}.rich-confirm input {
            background: var(--in-content-box-background);
            border: thin solid var(--in-content-box-border-color-mixed);
            color: var(--in-content-text-color);
          }
          ${common}.rich-confirm textarea:hover,
          ${common}.rich-confirm input:hover {
            border-color: var(--in-content-border-hover-mixed);
          }
          ${common}.rich-confirm textarea:focus,
          ${common}.rich-confirm input:focus {
            border-color: var(--in-content-border-focus);
            box-shadow: 0 0 0 1px var(--in-content-border-active),
                        0 0 0 4px var(--in-content-border-active-shadow);
          }

          ${common}.rich-confirm fieldset,
          ${common}.rich-confirm hr {
            border: thin solid var(--in-content-box-border-color-mixed);
          }

          ${common}.rich-confirm hr {
            border-width: thin 0 0 0;
          }

          ${common}.rich-confirm button,
          ${common}.rich-confirm select {
            background: var(--in-content-button-background-mixed);
            border: 0 none transparent;
            color: var(--in-content-text-color);
            margin: 4px;
          }
          ${common}.rich-confirm button:hover,
          ${common}.rich-confirm select:hover {
            background: var(--in-content-button-background-hover-mixed);
          }
          ${common}.rich-confirm button:focus,
          ${common}.rich-confirm select:focus {
            background: var(--in-content-button-background-active-mixed);
            box-shadow: 0 0 0 1px var(--in-content-border-active),
                        0 0 0 4px var(--in-content-border-active-shadow);
          }
          ${common}.rich-confirm option {
            background: var(--bg-color);
            color: var(--text-color);
          }
          ${common}.rich-confirm option:active,
          ${common}.rich-confirm option:focus {
            background: var(--in-content-item-selected);
          }
          ${common}.rich-confirm option:hover {
            background: var(--in-content-item-hover-mixed);
          }
        }

        ${common}.rich-confirm,
        ${common}.rich-confirm-row {
          align-items: center;
          display: flex;
          flex-direction: column;
          justify-content: center;
        }
        ${common}.rich-confirm:not(.simulation),
        ${common}.rich-confirm-row:not(.simulation) {
          align-items: stretch;
          bottom: 0;
          left: 0;
          position: fixed;
          right: 0;
          top: 0;
        }

        ${common}.rich-confirm {
          left:0;
          pointer-events: none;
          z-index: 999997;
        }
        ${common}.rich-confirm.popup-window,
        :root${common}.popup-window body {
          background: var(--bg-color);
        }
        ${common}.rich-confirm:not(.popup-window) {
          background: rgba(0, 0, 0, 0.45);
          opacity: 0;
          transition: opacity 250ms ease-out;
        }

        ${common}.rich-confirm.show {
          opacity: 1;
          pointer-events: auto;
        }

        ${common}.rich-confirm-row {
          z-index: 999998;
        }

        ${common}.rich-confirm-dialog {
          color: var(--text-color);
          font: message-box;
          overflow: hidden;
          padding: 1em;
          z-index: 999999;
        }
        /* Don't apply "auto" immediately because it can produce needless scrollbar even if all contents are visible without scrolling. */
        ${common}.rich-confirm.shown .rich-confirm-dialog {
          overflow: auto;
        }
        ${common}.rich-confirm.shown .rich-confirm-dialog.popup-window:not(.simulation) {
          display: flex;
          flex-direction: column;
          flex-grow: 1;
        }
        ${common}.rich-confirm-dialog:not(.popup-window) {
          background: var(--bg-color);
          box-shadow: 0.1em 0.1em 0.5em rgba(0, 0, 0, 0.65);
          margin: 0.5em;
          max-height: 90%;
          max-width: 90%;
        }

        ${common}.rich-confirm-content {
          white-space: pre-wrap;
        }
        ${common}.rich-confirm-content.popup-window:not(.simulation) {
          display: flex;
          flex-direction: column;
          flex-grow: 1;
          flex-shrink: 1;
        }

        ${common}.rich-confirm-buttons {
          align-items: center;
          display: flex;
          flex-direction: row;
          margin: 0.5em 0 0;
        }

        @media (min-width: 40em) {
          ${common}.rich-confirm-buttons.type-dialog button,
          ${common}.rich-confirm-buttons.type-common-dialog button {
            white-space: nowrap;
          }

          ${common}.rich-confirm-buttons.type-dialog {
            justify-content: flex-end;
          }
          ${common}.rich-confirm-buttons.type-dialog button + button {
            margin-left: 1em;
          }

          ${common}.rich-confirm-buttons.type-common-dialog {
            justify-content: center;
          }
          ${common}.rich-confirm-buttons.type-common-dialog button + button {
            margin-left: 1em;
          }

          ${common}.rich-confirm-buttons.type-dialog.mac,
          ${common}.rich-confirm-buttons.type-dialog.linux,
          ${common}.rich-confirm-buttons.type-common-dialog.mac,
          ${common}.rich-confirm-buttons.type-common-dialog.linux {
            justify-content: flex-start;
            flex-direction: row-reverse;
          }
          ${common}.rich-confirm-buttons.type-dialog.mac button + button,
          ${common}.rich-confirm-buttons.type-dialog.linux button + button,
          ${common}.rich-confirm-buttons.type-common-dialog.mac button + button,
          ${common}.rich-confirm-buttons.type-common-dialog.linux button + button {
            margin-right: 1em;
          }
        }

        /* popup type dialog always have horizontal buttons */
        ${common}.rich-confirm-buttons.popup-window.type-dialog button,
        ${common}.rich-confirm-buttons.popup-window.type-common-dialog button {
          white-space: nowrap;
        }
        ${common}.rich-confirm-buttons.popup-window.type-dialog {
          justify-content: flex-end;
        }
        ${common}.rich-confirm-buttons.popup-window.type-dialog button + button {
          margin-left: 1em;
        }
        ${common}.rich-confirm-buttons.popup-window.type-common-dialog {
          justify-content: center;
        }
        ${common}.rich-confirm-buttons.popup-window.type-common-dialog button + button {
          margin-left: 1em;
        }
        ${common}.rich-confirm-buttons.popup-window.type-dialog.mac,
        ${common}.rich-confirm-buttons.popup-window.type-dialog.linux,
        ${common}.rich-confirm-buttons.popup-window.type-common-dialog.mac,
        ${common}.rich-confirm-buttons.popup-window.type-common-dialog.linux {
          justify-content: flex-start;
          flex-direction: row-reverse;
        }
        ${common}.rich-confirm-buttons.popup-window.type-dialog.mac button + button,
        ${common}.rich-confirm-buttons.popup-window.type-dialog.linux button + button,
        ${common}.rich-confirm-buttons.popup-window.type-common-dialog.mac button + button,
        ${common}.rich-confirm-buttons.popup-window.type-common-dialog.linux button + button {
          margin-right: 1em;
        }

        ${common}.rich-confirm-buttons:not(.type-dialog):not(.type-common-dialog) {
          align-items: stretch;
          flex-direction: column;
        }
        @media (max-width: 40em) {
          ${common}.rich-confirm-buttons:not(.popup-window):not(.type-dialog).type-common-dialog {
            align-items: stretch;
            flex-direction: column;
          }
        }

        ${common}.rich-confirm button {
          -moz-appearance: button;
          font: message-box;
          text-align: center;
        }

        ${common}.rich-confirm-buttons:not(.type-dialog):not(.type-common-dialog) button {
          display: block;
          margin-bottom: 0.2em;
          padding: 0.4em;
          width: 100%;
        }
        @media (max-width: 40em) {
          ${common}.rich-confirm-buttons:not(.popup-window):not(.type-dialog).type-common-dialog button {
            display: block;
            margin-bottom: 0.2em;
            padding: 0.4em;
            width: 100%;
          }
        }

        ${common}.rich-confirm-check-label {
          display: flex;
          flex-direction: row;
          margin-top: 0.5em;
        }

        ${common}.rich-confirm-check-label.hidden {
          display: none;
        }

        ${common}.rich-confirm .accesskey {
          text-decoration: underline;
        }


        ${common}.rich-confirm.simulation {
          max-height: 0 !important;
          max-width: 0 !important;
          overflow: hidden !important;
          visibility: hidden !important;
        }
        ${common}.rich-confirm-row.simulation {
          position: static !important;
        }
        ${common}.rich-confirm-dialog.simulation {
          border:  1px solid;
        }
      `;
      document.head.appendChild(this.style);

      const range = document.createRange();
      range.selectNodeContents(document.body);
      range.collapse(false);
      const commonClass = [
        this.commonClass,
        this.params.popup ? 'popup-window' : '',
        this.params.simulation ? 'simulation' : '',
        this.params.type ? `type-${this.params.type}` : '',
        /win/i.test(navigator.platform) ? 'windows' :
          /mac/i.test(navigator.platform) ? 'mac' :
            /linux/i.test(navigator.platform) ? 'linux' :
              ''
      ].join(' ');
      const uniqueId = `created-at-${Date.now()}-${Math.floor(Math.random() * Math.pow(2, 24))}`;
      const fragment = range.createContextualFragment(`
        <div class="rich-confirm ${commonClass} ${uniqueId}">
          <div class="rich-confirm-row ${commonClass}">
            <div class="rich-confirm-dialog ${commonClass}" role="dialog">
              <div class="rich-confirm-content ${commonClass}"></div>
              <label class="rich-confirm-check-label ${commonClass}">
                <input type="checkbox"
                       class="rich-confirm-check-checkbox ${commonClass}">
                <span class="rich-confirm-check-message ${commonClass}"></span>
              </label>
              <div class="rich-confirm-buttons ${commonClass}"></div>
            </div>
          </div>
        </div>
      `);
      range.insertNode(fragment);
      range.detach();
      this.ui = document.querySelector(`.rich-confirm.${this.commonClass}.${uniqueId}`);
    }

    getNextFocusedNodeByAccesskey(key) {
      for (const attribute of ['accesskey', 'data-access-key', 'data-sub-access-key']) {
        const current = this.dialog.querySelector(':focus');
        const condition = `[${attribute}="${key.toLowerCase()}"]`;
        const nextNode = this.getNextNode(current, condition);
        if (nextNode)
          return nextNode;
      }
      return null;
    }

    getNextNode(base, condition = '') {
      const matchedNodes = [...this.dialog.querySelectorAll(condition)];
      const currentIndex = matchedNodes.indexOf(base);
      const nextNode = currentIndex == -1 || currentIndex == matchedNodes.index - 1 ?
        matchedNodes[0] : matchedNodes[currentIndex + 1];
      if (nextNode && window.getComputedStyle(nextNode, null).display == 'none')
        return this.getNextNode(nextNode, condition);
      return nextNode;
    }

    updateAccessKey(element) {
      const ACCESS_KEY_MATCHER = /(&([^\s]))/i;
      const label = element.textContent || (/^(button|submit|reset)$/i.test(element.type) && element.value) || '';
      const matchedKey = element.accessKey ?
        label.match(new RegExp(`((${element.accessKey}))`, 'i')) :
        label.match(ACCESS_KEY_MATCHER);
      const accessKey = element.accessKey || (matchedKey && matchedKey[2]);
      if (accessKey) {
        element.accessKey = element.dataset.accessKey = accessKey.toLowerCase();
        if (matchedKey &&
            !/^(input|textarea)$/i.test(element.localName)) {
          const textNode = this.evaluateXPath(
            `child::node()[contains(self::text(), "${matchedKey[1]}")]`,
            element,
            XPathResult.FIRST_ORDERED_NODE_TYPE
          ).singleNodeValue;
          if (textNode) {
            const range = document.createRange();
            const startPosition = textNode.nodeValue.indexOf(matchedKey[1]);
            range.setStart(textNode, startPosition);
            range.setEnd(textNode, startPosition + matchedKey[1].length);
            range.deleteContents();
            const accessKeyNode = document.createElement('span');
            accessKeyNode.classList.add('accesskey');
            accessKeyNode.textContent = matchedKey[2];
            range.insertNode(accessKeyNode);
            range.detach();
          }
        }
      }
      else if (/^([^\s])/i.test(label))
        element.dataset.subAccessKey = RegExp.$1.toLowerCase();
      else
        element.dataset.accessKey = element.dataset.subAccessKey = null;
    }

    evaluateXPath(expression, context, type) {
      if (!type)
        type = XPathResult.ORDERED_NODE_SNAPSHOT_TYPE;
      try {
        return (context.ownerDocument || context).evaluate(
          expression,
          (context || document),
          null,
          type,
          null
        );
      }
      catch(_e) {
        return {
          singleNodeValue: null,
          snapshotLength:  0,
          snapshotItem:    function() {
            return null
          }
        };
      }
    }

    async show({ onShown } = {}) {
      this.buildUI();
      await new Promise((resolve, _reject) => setTimeout(resolve, 0));

      const range = document.createRange();

      if (this.params.content) {
        range.selectNodeContents(this.content);
        range.collapse(false);
        const fragment = range.createContextualFragment(this.params.content);
        range.insertNode(fragment);
        for (const element of this.content.querySelectorAll('[accesskey]')) {
          this.updateAccessKey(element);
        }
      }
      else if (this.params.message) {
        this.content.textContent = this.params.message;
      }

      if (this.params.checkMessage) {
        this.checkMessage.textContent = this.params.checkMessage;
        this.checkCheckbox.checked = !!this.params.checked;
        this.checkContainer.classList.remove('hidden');
      }
      else {
        this.checkContainer.classList.add('hidden');
      }

      range.selectNodeContents(this.buttonsContainer);
      range.deleteContents();
      const buttons = document.createDocumentFragment();
      for (const label of this.params.buttons) {
        const button = document.createElement('button');
        button.textContent = label;
        button.setAttribute('title', label);
        buttons.appendChild(button);
        this.updateAccessKey(button);
      }
      range.insertNode(buttons);

      this.ui.addEventListener('click', this.onClick);
      window.addEventListener('keydown', this.onKeyDown, true);
      window.addEventListener('keyup', this.onKeyUp, true);
      window.addEventListener('contextmenu', this.onContextMenu, true);
      window.addEventListener('pagehide', this.onUnload);
      window.addEventListener('beforeunload', this.onUnload);

      const targets = this.focusTargets.filter(target => target != this.checkCheckbox);
      targets[0].focus();

      range.detach();

      if (typeof this.params.onShown == 'function') {
        try {
          await this.params.onShown(this.content, this.params.inject || {});
        }
        catch(error) {
          console.error(error);
        }
      }
      else if (Array.isArray(this.params.onShown)) {
        for (const onShownPart of this.params.onShown) {
          if (typeof onShownPart != 'function')
            continue;
          try {
            await onShownPart(this.content, this.params.inject || {});
          }
          catch(error) {
            console.error(error);
          }
        }
      }

      this.ui.querySelector('.rich-confirm-dialog').setAttribute('aria-modal', !!this.params.modal);
      this.ui.classList.add('show');

      if (typeof onShown == 'function') {
        try {
          await onShown(this.content, this.params.inject || {});
        }
        catch(error) {
          console.error(error);
        }
      }
      else if (Array.isArray(onShown)) {
        for (const onShownPart of onShown) {
          if (typeof onShownPart != 'function')
            continue;
          try {
            await onShownPart(this.content, this.params.inject || {});
          }
          catch(error) {
            console.error(error);
          }
        }
      }

      setTimeout(() => {
        if (!this.ui ||
            !this.ui.classList)
          return;
        // Apply overflow:auto after all contents are correctly rendered.
        this.ui.classList.add('shown');
      }, 10);

      return new Promise((resolve, reject) => {
        this._resolve = resolve;
        this._rejecte = reject;
      });
    }

    async hide() {
      this.ui.classList.remove('show');
      if (typeof this.params.onHidden == 'function') {
        try {
          this.params.onHidden(this.content, this.params.inject || {});
        }
        catch(_error) {
        }
      }
      this.ui.removeEventListener('click', this.onClick);
      window.removeEventListener('keydown', this.onKeyDown, true);
      window.removeEventListener('keyup', this.onKeyUp, true);
      window.removeEventListener('contextmenu', this.onContextMenu, true);
      window.removeEventListener('pagehide', this.onUnload);
      window.removeEventListener('beforeunload', this.onUnload);
      delete this._resolve;
      delete this._rejecte;
      const ui = this.ui;
      const style = this.style;
      delete this.ui;
      delete this.style;
      return new Promise((resolve, _reject) => {
        window.setTimeout(() => {
          // remove elements after animation is finished
          ui.parentNode.removeChild(ui);
          style.parentNode.removeChild(style);
        }, 1000);
        resolve();
      });
    }

    dismiss() {
      const resolve = this._resolve;
      const result = {
        buttonIndex: -1,
        checked: !!this.params.checkMessage && this.checkCheckbox.checked
      };
      return this.hide().then(() => resolve(result));
    }

    onClick(event) {
      let target = event.target;
      if (target.nodeType == Node.TEXT_NODE)
        target = target.parentNode;

      if (target.closest(`.rich-confirm-content.${this.commonClass}`) &&
          target.closest('input, textarea, select, button'))
        return;

      if (event.button != 0) {
        event.stopPropagation();
        event.preventDefault();
        return;
      }

      const button = target.closest('button');
      if (button) {
        event.stopPropagation();
        event.preventDefault();
        const buttonIndex = Array.from(this.buttonsContainer.childNodes).indexOf(button);
        const values = {};
        for (const field of this.content.querySelectorAll('[id], [name]')) {
          let value = null;
          if (field.matches('input[type="checkbox"]')) {
            value = field.checked;
          }
          else if (field.matches('input[type="radio"]')) {
            if (field.checked)
              value = field.value;
          }
          else if ('value' in field.dataset) {
            value = field.dataset.value;
          }
          else {
            value = field.value;
          }
          values[field.id || field.name] = value;
        }
        const resolve = this._resolve;
        const result = {
          buttonIndex,
          values,
          checked: !!this.params.checkMessage && this.checkCheckbox.checked
        };
        this.hide().then(() => resolve(result));
        return;
      }

      if (!this.params.popup &&
          !target.closest(`.rich-confirm-dialog.${this.commonClass}`)) {
        event.stopPropagation();
        event.preventDefault();
        this.dismiss();
      }
    }

    onKeyDown(event) {
      let target = event.target;
      if (target.nodeType == Node.TEXT_NODE)
        target = target.parentNode;
      const onContent = target.closest(`.rich-confirm-content.${this.commonClass}`);

      switch (event.key) {
        case 'ArrowUp':
        case 'ArrowLeft':
        case 'PageUp':
          if (onContent)
            break;
          event.stopPropagation();
          event.preventDefault();
          this.advanceFocus(-1);
          break;

        case 'ArrowDown':
        case 'ArrowRight':
        case 'PageDown':
          if (onContent)
            break;
          event.stopPropagation();
          event.preventDefault();
          this.advanceFocus(1);
          break;

        case 'Home':
          if (onContent)
            break;
          event.stopPropagation();
          event.preventDefault();
          this.focusTargets[0].focus();
          break;

        case 'End':
          if (onContent)
            break;
          event.stopPropagation();
          event.preventDefault();
          const targets = this.focusTargets;
          targets[targets.length-1].focus();
          break;

        case 'Tab':
          event.stopPropagation();
          event.preventDefault();
          this.advanceFocus(event.shiftKey ? -1 : 1);
          break;

        case 'Escape':
          event.stopPropagation();
          event.preventDefault();
          this.dismiss();
          break;

        case 'Enter':
          if (onContent && !target.closest('textarea')) {
            event.stopPropagation();
            event.preventDefault();
            this.buttonsContainer.firstChild.click();
          }
          break;

        default: {
          const currentFocused = this.dialog.querySelector(':focus');
          const needAccelKey = (
            currentFocused &&
            (currentFocused.localName.toLowerCase() == 'textarea' ||
             (currentFocused.localName.toLowerCase() == 'input' &&
              /^(date|datetime|datetime-local|email|file|month|number|password|search|tel|text|time|url|week)$/i.test(currentFocused.type)))
          );
          if ((!needAccelKey || event.altKey) &&
              !event.ctrlKey &&
              !event.shiftKey &&
              !event.metaKey &&
              event.key.length == 1) {
            const node = this.getNextFocusedNodeByAccesskey(event.key);
            if (node && typeof node.focus == 'function') {
              node.focus();
              const nextNode = this.getNextFocusedNodeByAccesskey(event.key);
              if ((!nextNode || nextNode == node) &&
                  typeof node.click == 'function')
                node.click();
            }
          }
        }; return;
      }
    }

    onKeyUp(event) {
      switch (event.key) {
        case 'ArrowUp':
        case 'ArrowLeft':
        case 'PageUp':
        case 'ArrowDown':
        case 'ArrowRight':
        case 'PageDown':
        case 'Home':
        case 'End':
        case 'Tab':
        case 'Escape':
          event.stopPropagation();
          event.preventDefault();
          break;

        default:
          return;
      }
    }

    onContextMenu(event) {
      let target = event.target;
      if (target.nodeType == Node.TEXT_NODE)
        target = target.parentNode;
      const onContent = target.closest(`.rich-confirm-content.${this.commonClass}`);
      if (!onContent || !target.closest('input, textarea')) {
        event.preventDefault();
        event.stopPropagation();
        event.stopImmediatePropagation();
      }
    }

    onUnload() {
      this.dismiss();
    }

    advanceFocus(direction) {
      const focusedItem = this.ui.querySelector(':focus');
      console.log('focusedItem ', focusedItem);
      const targets = this.focusTargets;
      console.log('focusTargets ', targets);
      const index = focusedItem ? targets.indexOf(focusedItem) : -1;
      if (direction < 0) { // backward
        const nextFocused = index < 0 ? targets[targets.length-1] :
          targets[index == 0 ? targets.length-1 : index-1];
        nextFocused.focus();
      }
      else { // forward
        const nextFocused = index < 0 ? targets[0] :
          targets[index == targets.length-1 ? 0 : index+1];
        nextFocused.focus();
      }
    }

    static async show(params) {
      const confirm = new this(params);
      return confirm.show();
    }

    static async showInTab(tabId, params) {
      if (!params) {
        params = tabId;
        tabId = (await browser.tabs.getCurrent()).id;
      }
      let onMessage;
      const promisedResult = new Promise((resolve, _reject) => {
        onMessage = (message, _sender) => {
          if (!message ||
              typeof message != 'object' ||
              message.uniqueKey != this.uniqueKey)
            return;

          switch (message.type) {
            case 'rich-confirm-dialog-shown':
              if (typeof params.onReady == 'function') {
                try {
                  params.onReady({
                    width:  message.dialogWidth,
                    height: message.dialogHeight
                  });
                }
                catch(error) {
                  console.error(error);
                }
              }
              break;

            case 'rich-confirm-dialog-complete':
              resolve(message.result);
              break;
          }
        };
        browser.runtime.onMessage.addListener(onMessage);
      });
      try {
        await browser.tabs.executeScript(tabId, {
          code: `
            if (!window.RichConfirm)
               (${defineRichConfirm.toString()})(${this.uniqueKey});
          `,
          matchAboutBlank: true,
          runAt:           'document_start'
        });
        const transferableParams = { ...params };
        const injectTransferable = [];
        const inject = params.inject || {};
        delete transferableParams.inject;
        for (const key in params.inject) {
          const value = inject[key];
          const transferable = (
            value &&
            typeof value == 'function' &&
            typeof value.toString == 'function'
          ) ? value.toString() : JSON.stringify(value);
          injectTransferable.push(`${JSON.stringify(key)} : ${transferable}`);
        }
        const stringifyOnShown = onShown => {
          if (Array.isArray(onShown))
            return `[${onShown.map(stringifyOnShown).join(',')}]`;
          return typeof onShown == 'function' ?
            onShown.toString()
              .replace(/^\s*(async\s+)?function/, '$1')
              .replace(/^\s*(async\s+)?/, '$1 function ')
              .replace(/^\s*(async\s+)?function ((?:\([^=\)]*\)|[^\(\)=]+)\s*=>\s*\{)/, '$1 $2') :
            '() => {}';
        };
        const originalOnShown = stringifyOnShown(params.onShown);
        delete transferableParams.onShown;
        browser.tabs.executeScript(tabId, {
          code: `
            delete window.RichConfirm.result;
            (async (originalOnShown, inject) => {
              const params = ${JSON.stringify(transferableParams)};
              const confirm = new RichConfirm({
                ...params,
                inject,
                async onShown(content, inject) {
                  if (!Array.isArray(originalOnShown))
                    originalOnShown = [originalOnShown];
                  for (const originalOnShownPart of originalOnShown) {
                    try {
                      if (typeof originalOnShownPart == 'function')
                        await originalOnShownPart(content, inject);
                    }
                    catch(error) {
                      console.error(error);
                    }
                  }
                }
              });
              const result = await confirm.show({
                onShown(content, _injected) {
                  const dialog = content.parentNode;
                  const rect   = dialog.getBoundingClientRect();
                  const style  = window.getComputedStyle(dialog, null);
                  // End padding is not included in the scrillable size,
                  // so we manually add them.
                  const rightPadding  = dialog.scrollLeftMax > 0 && parseFloat(style.getPropertyValue('padding-right')) || 0;
                  const bottomPadding = dialog.scrollTopMax > 0 && parseFloat(style.getPropertyValue('padding-bottom')) || 0;
                  browser.runtime.sendMessage({
                    type:         'rich-confirm-dialog-shown',
                    uniqueKey:    ${JSON.stringify(this.uniqueKey)},
                    dialogWidth:  rect.width + dialog.scrollLeftMax + rightPadding,
                    dialogHeight: rect.height + dialog.scrollTopMax + bottomPadding
                  });
                }
              });
              browser.runtime.sendMessage({
                type:      'rich-confirm-dialog-complete',
                uniqueKey: ${JSON.stringify(this.uniqueKey)},
                result
              });
            })(
              (${originalOnShown}),
              {${injectTransferable.join(',')}}
            );
          `,
          matchAboutBlank: true,
          runAt:           'document_start'
        });
        // Don't return the promise directly here, instead await it
        // because the "finally" block must be processed after
        // the promise is resolved.
        const result = await promisedResult;
        return result;
      }
      catch(error) {
        console.error(error, error.stack);
        return {
          buttonIndex: -1
        };
      }
      finally {
        if (browser.runtime.onMessage.hasListener(onMessage))
          browser.runtime.onMessage.removeListener(onMessage);
      }
    }

    static async showInPopup(winId, params) {
      let ownerWin;
      if (!params) {
        params = winId;
        ownerWin = await browser.windows.getLastFocused({});
      }
      else {
        ownerWin = await browser.windows.get(winId);
      }

      const type = this.DIALOG_READY_NOTIFICATION_TYPE;
      const tryRepositionDialogToCenterOfOwner = this._tryRepositionDialogToCenterOfOwner;
      browser.runtime.onMessage.addListener(function onMessage(message, sender) {
        switch (message.type) {
          case type:
            browser.runtime.onMessage.removeListener(onMessage);
            tryRepositionDialogToCenterOfOwner({
              ...message,
              dialogWindowId: sender.tab.windowId,
            });
            break;
        }
      });

      return this._showInPopupInternal(ownerWin, {
        ...params,
        inject: {
          ...(params.inject || {}),
          __RichConfirm__reportScreenMessageType: type,
          __RichConfirm__ownerWindowId: ownerWin.id,
        },
        onShown: [
          ...(!params.onShown ? [] : Array.isArray(params.onShown) ? params.onShown : [params.onShown]),
          (container, { __RichConfirm__reportScreenMessageType, __RichConfirm__ownerWindowId }) => {
            setTimeout(() => {
              // We cannot move this window by this callback function, thus I just send
              // a request to update window position.
              browser.runtime.sendMessage({
                type:          __RichConfirm__reportScreenMessageType,
                ownerWindowId: __RichConfirm__ownerWindowId,
                availLeft:     screen.availLeft,
                availTop:      screen.availTop,
                availWidth:    screen.availWidth,
                availHeight:   screen.availHeight,
              });
            }, 0);
          },
        ],
      });
    }

    static async _tryRepositionDialogToCenterOfOwner({ dialogWindowId, ownerWindowId, availLeft, availTop, availWidth, availHeight }) {
      const [dialogWin, ownerWin] = await Promise.all([
        browser.windows.get(dialogWindowId),
        browser.windows.get(ownerWindowId),
      ]);
      const placedOnOwner = (
        dialogWin.left + dialogWin.width - (dialogWin.width / 2) < ownerWin.left &&
        dialogWin.top + dialogWin.height - (dialogWin.height / 2) < ownerWin.top &&
        dialogWin.left + (dialogWin.width / 2) < ownerWin.left + ownerWin.width &&
        dialogWin.top + (dialogWin.height / 2) < ownerWin.top + ownerWin.height
      );
      const placedInsideViewArea = (
        dialogWin.left >= availLeft &&
        dialogWin.top >= availTop &&
        dialogWin.left + dialogWin.width <= availLeft + availWidth &&
        dialogWin.top + dialogWin.height <= availTop + availHeight
      );
      if (placedOnOwner && placedInsideViewArea)
        return;

      const top  = ownerWin.top + Math.round((ownerWin.height - dialogWin.height) / 2);
      const left = ownerWin.left + Math.round((ownerWin.width - dialogWin.width) / 2);
      return browser.windows.update(dialogWin.id, {
        left: Math.min(availLeft + availWidth - dialogWin.width, Math.max(availLeft, left)),
        top:  Math.min(availTop + availHeight - dialogWin.height, Math.max(availTop, top)),
      });
    }

    static async _showInPopupInternal(ownerWin, params) {
      const minWidth  = Math.max(ownerWin.width, Math.ceil(screen.availWidth / 3));
      const minHeight = Math.max(ownerWin.height, Math.ceil(screen.availHeight / 3));

      const simulation = new this({
        ...params,
        popup: true,
        simulation: true
      });
      simulation.buildUI();
      const simulatedContainer = simulation.ui.querySelector('.rich-confirm-row');
      simulatedContainer.style.minWidth  = `${minWidth}px`;
      simulatedContainer.style.minHeight = `${minHeight}px`;
      await new Promise((resolve, _reject) => {
        simulation.show({
          onShown() {
            setTimeout(() => {
              resolve();
            }, 0);
          }
        });
      });
      const simulatedDialog = simulation.ui.querySelector('.rich-confirm-dialog');
      const simulatedRect   = simulatedDialog.getBoundingClientRect();

      // Safe guard for scrollbar due to unexpected line breaks
      const safetyFactor  = 1.05;
      const simulatedSize = {
        width:  Math.ceil(simulatedRect.width * safetyFactor),
        height: Math.ceil(simulatedRect.height * safetyFactor)
      };
      simulation.hide();
      // This dimension is not accurate because we must think about
      // the size of the window frame, but currently I don't know how to
      // calculate it here...

      simulatedSize.top  = ownerWin.top + Math.floor((ownerWin.height - simulatedSize.height) / 2);
      simulatedSize.left = ownerWin.left + Math.floor((ownerWin.width - simulatedSize.width) / 2);

      const url     = params.url || 'about:blank';
      const fullUrl = /^about:/.test(url) || /^\w+:\/\//.test(url) ?
        url :
        `moz-extension://${location.host}/${url.replace(/^\//, '')}`;
      const win = await browser.windows.create({
        url:    fullUrl,
        type:   'popup',
        ...simulatedSize
      });
      // Due to a Firefox's bug we cannot open popup type window
      // at specified position.
      // https://bugzilla.mozilla.org/show_bug.cgi?id=1271047
      // Thus we need to move the window immediately after it is opened.
      if (win.left + win.width - (win.width / 2) <= ownerWin.left ||
          win.top + win.height - (win.height / 2) <= ownerWin.top ||
          win.left + (win.width / 2) >= ownerWin.left + ownerWin.width ||
          win.top + (win.height / 2) >= ownerWin.top + ownerWin.height) {
        // But, such a move will produce an annoying flash.
        // So, I grudgingly accept the position of the dialog placed
        // if the popup (partially or fully) covers the owner window.
        browser.windows.update(win.id, {
          top:  simulatedSize.top,
          left: simulatedSize.left
        });
      }
      const activeTab = win.tabs.find(tab => tab.active);

      const onFocusChanged = !params.modal ? null : windowId => {
        if (windowId == ownerWin.id)
          browser.windows.update(win.id, { focused: true });
      };
      if (onFocusChanged)
        browser.windows.onFocusChanged.addListener(onFocusChanged);

      let onClosed;
      const promisedDismissed = new Promise((resolve, _reject) => {
        onClosed = windowId => {
          if (windowId == win.id) {
            win.closed = true;
            resolve({ buttonIndex: -1 });
          }
        };
      });
      browser.windows.onRemoved.addListener(onClosed);

      const result = await Promise.race([
        promisedDismissed,
        (async () => {
          try {
            const frameSize = await new Promise((resolve, _reject) => {
              let timeout;
              const code = `(() => {
                ${params.title ? 'document.title = ' + JSON.stringify(params.title) +';' : ''}
                const classList = document.documentElement.classList;
                classList.add('rich-confirm-${uniqueKey}');
                classList.add('popup-window');
                return {
                  width:  window.outerWidth - window.innerWidth,
                  height: window.outerHeight - window.innerHeight,
                  url:    location.href
                };
              })();`;
              const onTabUpdated = (tabId, updateInfo, _tab) => {
                if (updateInfo.status != 'complete' ||
                    !browser.tabs.onUpdated.hasListener(onTabUpdated))
                  return;
                if (timeout)
                  clearTimeout(timeout);
                browser.tabs.executeScript(tabId, {
                  code,
                  matchAboutBlank: true,
                  runAt:           'document_start'
                }).then(results => {
                  if (results[0].url != fullUrl)
                    return;
                  browser.tabs.onUpdated.removeListener(onTabUpdated);
                  resolve(results[0]);
                });
              };
              timeout = setTimeout(() => {
                if (!browser.tabs.onUpdated.hasListener(onTabUpdated))
                  return;
                timeout = null;
                browser.tabs.executeScript(activeTab.id, {
                  code,
                  matchAboutBlank: true,
                  runAt:           'document_start'
                }).then(results => {
                  if (results[0].url != fullUrl)
                    return;
                  browser.tabs.onUpdated.removeListener(onTabUpdated);
                  resolve(results[0]);
                });
              }, 500);
              browser.tabs.onUpdated.addListener(onTabUpdated, {
                properties: ['status'],
                tabId:      activeTab.id
              });
            });

            if (typeof browser.tabs.setZoom == 'function')
              await browser.tabs.setZoom(activeTab.id, 1);
            return this.showInTab(activeTab.id, {
              ...params,
              popup: true,
              onReady(dialogSize) {
                const actualWidth  = Math.min(
                  Math.ceil(dialogSize.width + frameSize.width),
                  Math.max(
                    ownerWin.left + ownerWin.width - simulatedSize.left,
                    minWidth
                  )
                );
                const actualHeight = Math.min(
                  Math.ceil(dialogSize.height + frameSize.height),
                  Math.max(
                    ownerWin.top + ownerWin.height - simulatedSize.top,
                    minHeight
                  )
                );
                browser.windows.update(win.id, {
                  width:  actualWidth,
                  height: actualHeight,
                  // We should reposition the dialog at truly center of the
                  // owner window, but it will produce an annoying slip of
                  // the window, so I give up for now.
                  //top:    Math.floor(ownerWin.top + ((ownerWin.height - actualHeight) / 2)),
                  //left:   Math.floor(ownerWin.left + ((ownerWin.width - actualWidth) / 2))
                });
              }
            });
          }
          catch(error) {
            console.error(error);
            return null;
          }
        })()
      ]);

      if (onFocusChanged)
        browser.windows.onFocusChanged.removeListener(onFocusChanged);
      browser.windows.onRemoved.removeListener(onClosed);

      if (!win.closed) {
        // A window closed with a blank page won't appear
        // in the "Recently Closed Windows" list.
        browser.tabs.executeScript(activeTab.id, {
          code:            `location.replace('about:blank')`,
          matchAboutBlank: true,
          runAt:           'document_start'
        }).then(() => {
          browser.windows.remove(win.id);
        });
      }

      return result;
    }
  };
  RichConfirm.uniqueKey = RichConfirm.prototype.uniqueKey = uniqueKey;
  RichConfirm.DIALOG_READY_NOTIFICATION_TYPE = `__RichConfirm_${uniqueKey}__confirmation-dialog-ready`;
  window.RichConfirm = RichConfirm;
  return true; // this is required to run this script as a content script
})(parseInt(Math.random() * Math.pow(2, 16)));
export default RichConfirm;
