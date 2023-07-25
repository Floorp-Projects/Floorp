/*
 license: The MIT License, Copyright (c) 2018-2021 YUKI "Piro" Hiroshi
 original:
   https://github.com/piroor/webextensions-lib-menu-ui
*/
'use strict';

{
  class MenuUI {
    static $wait(timeout) {
      return new Promise((resolve, _reject) => {
        setTimeout(resolve, timeout);
      });
    }

    // XPath Utilities
    static $hasClass(className) {
      return `contains(concat(" ", normalize-space(@class), " "), " ${className} ")`;
    };

    static $evaluateXPath(expression, context, type) {
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

    static $getArrayFromXPathResult(result) {
      const max   = result.snapshotLength;
      const array = new Array(max);
      if (!max)
        return array;

      for (let i = 0; i < max; i++) {
        array[i] = result.snapshotItem(i);
      }
      return array;
    }

    constructor(params = {}) {
      this.$lastHoverItem   = null;
      this.$lastFocusedItem = null;
      this.$mouseDownFired  = false;

      this.root              = params.root;
      this.onCommand         = params.onCommand || (() => {});
      this.onShown           = params.onShown || (() => {});
      this.onHidden          = params.onHidden || (() => {});
      this.animationDuration = params.animationDuration || 150;
      this.subMenuOpenDelay  = params.subMenuOpenDelay || 300;
      this.subMenuCloseDelay = params.subMenuCloseDelay || 300;
      this.appearance        = params.appearance || 'menu';
      this.incrementalSearch = params.incrementalSearch || false;
      this.incrementalSearchTimeout = params.incrementalSearchTimeout || 1000;

      this.$onBlur            = this.$onBlur.bind(this);
      this.$onMouseOver       = this.$onMouseOver.bind(this);
      this.$onMouseDown       = this.$onMouseDown.bind(this);
      this.$onMouseUp         = this.$onMouseUp.bind(this);
      this.$onClick           = this.$onClick.bind(this);
      this.$onKeyDown         = this.$onKeyDown.bind(this);
      this.$onKeyUp           = this.$onKeyUp.bind(this);
      this.$onTransitionEnd   = this.$onTransitionEnd.bind(this);
      this.$onContextMenu     = this.$onContextMenu.bind(this);

      if (!this.root.id)
        this.root.id = `MenuUI-root-${this.$uniqueKey}-${parseInt(Math.random() * Math.pow(2, 16))}`;

      this.root.classList.add(this.$commonClass);
      this.root.classList.add('menu-ui');
      this.root.classList.add(this.appearance);
      this.root.setAttribute('role', 'menu');

      this.$screen = document.createElement('div');
      this.$screen.classList.add(this.$commonClass);
      this.$screen.classList.add('menu-ui-blocking-screen');
      this.root.parentNode.insertBefore(this.$screen, this.root.nextSibling);

      this.$marker = document.createElement('span');
      this.$marker.classList.add(this.$commonClass);
      this.$marker.classList.add('menu-ui-marker');
      this.$marker.classList.add(this.appearance);
      this.root.parentNode.insertBefore(this.$marker, this.root.nextSibling);

      this.$lastKeyInputAt = -1;
      this.$incrementalSearchString = '';
    }

    get opened() {
      return this.root.classList.contains('open');
    }

    $updateAccessKey(item) {
      const ACCESS_KEY_MATCHER = /(&([^\s]))/i;

      const title = item.getAttribute('title');
      if (title)
        item.setAttribute('title', title.replace(ACCESS_KEY_MATCHER, '$2'));

      const label = MenuUI.$evaluateXPath('child::text()', item, XPathResult.STRING_TYPE).stringValue;
      item.dataset.lowerCasedText = label.toLowerCase();

      const matchedKey = label.match(ACCESS_KEY_MATCHER);
      if (matchedKey) {
        const textNode = MenuUI.$evaluateXPath(
          `child::node()[contains(self::text(), "${matchedKey[1]}")]`,
          item,
          XPathResult.FIRST_ORDERED_NODE_TYPE
        ).singleNodeValue;
        if (textNode) {
          const range = document.createRange();
          const startPosition = textNode.nodeValue.indexOf(matchedKey[1]);
          range.setStart(textNode, startPosition);
          range.setEnd(textNode, startPosition + 2);
          range.deleteContents();
          const accessKeyNode = document.createElement('span');
          accessKeyNode.classList.add('accesskey');
          accessKeyNode.textContent = matchedKey[2];
          range.insertNode(accessKeyNode);
          range.detach();
        }
        item.dataset.accessKey = matchedKey[2].toLowerCase();
      }
      else if (/^([^\s])/i.test(item.textContent))
        item.dataset.subAccessKey = RegExp.$1.toLowerCase();
      else
        item.dataset.accessKey = item.dataset.subAccessKey = null;
    }

    async open(options = {}) {
      if (this.closeTimeout) {
        clearTimeout(this.closeTimeout);
        delete this.closeTimeout;
        this.$onClosed();
      }
      this.canceller = options.canceller;
      this.$mouseDownAfterOpen = false;
      this.$lastFocusedItem = null;
      this.$lastHoverItem = null;
      this.anchor = options.anchor;
      this.$updateItems(this.root);
      this.root.classList.add('open');
      this.$screen.classList.add('open');
      this.$marker.classList.remove('top');
      this.$marker.classList.remove('bottom');
      if (this.anchor) {
        this.anchor.classList.add('open');
        this.$marker.style.transition = `opacity ${this.animationDuration}ms ease-out`;
        this.$marker.classList.add('open');
      }
      this.$updatePositions(this.root, options);
      this.onShown();
      return new Promise(async (resolve, _reject) => {
        await MenuUI.$wait(0);
        if (this.$tryCancelOpen()) {
          this.close().then(resolve);
          return;
        }
        await MenuUI.$wait(this.animationDuration);
        if (this.$tryCancelOpen()) {
          this.close().then(resolve);
          return;
        }
        this.root.parentNode.addEventListener('mouseover', this.$onMouseOver);
        this.root.addEventListener('transitionend', this.$onTransitionEnd);
        window.addEventListener('contextmenu', this.$onContextMenu, { capture: true });
        window.addEventListener('mousedown', this.$onMouseDown, { capture: true });
        window.addEventListener('mouseup', this.$onMouseUp, { capture: true });
        window.addEventListener('click', this.$onClick, { capture: true });
        window.addEventListener('keydown', this.$onKeyDown, { capture: true });
        window.addEventListener('keyup', this.$onKeyUp, { capture: true });
        window.addEventListener('blur', this.$onBlur, { capture: true });
        resolve();
      });
    }

    $tryCancelOpen() {
      if (!(typeof this.canceller == 'function'))
        return false;
      try {
        return this.canceller();
      }
      catch(_e) {
      }
      return false;
    }

    updateMenuItem(item) {
      this.$updateItems(item);
      const submenu = item.querySelector('ul');
      if (submenu)
        this.$updatePositions(submenu);
    }

    $updateItems(parent) {
      parent.setAttribute('role', 'menu');
      for (const item of parent.querySelectorAll('li:not(.separator)')) {
        item.setAttribute('tabindex', -1);
        item.classList.remove('open');

        if (item.classList.contains('checkbox'))
          item.setAttribute('role', 'menuitemcheckbox');
        else if (item.classList.contains('radio'))
          item.setAttribute('role', 'menuitemradio');
        else if (item.classList.contains('separator'))
          item.setAttribute('role', 'separator');
        else
          item.setAttribute('role', 'menuitem');

        if (item.matches('.checked, .radio')) {
          if (item.classList.contains('checked'))
            item.setAttribute('aria-checked', 'true');
          else
            item.setAttribute('aria-checked', 'false');
        }
        else {
          item.removeAttribute('aria-checked');
        }

        this.$updateAccessKey(item);
        const icon = item.querySelector('span.icon') || document.createElement('span');
        if (!icon.parentNode) {
          icon.classList.add('icon');
          item.insertBefore(icon, item.firstChild);
        }
        if (item.dataset.icon) {
          if (item.dataset.iconColor) {
            item.style.backgroundImage = '';
            icon.style.backgroundColor = item.dataset.iconColor;
            icon.style.mask            = `url(${JSON.stringify(item.dataset.icon)}) no-repeat center / 100%`;
          }
          else {
            item.style.backgroundImage = `url(${JSON.stringify(item.dataset.icon)})`;
            icon.style.backgroundColor =
              icon.style.mask = '';
          }
        }
        else {
          item.style.backgroundImage =
            icon.style.backgroundColor =
            icon.style.mask = '';
        }
        if (item.querySelector('ul'))
          item.classList.add('has-submenu');
        else
          item.classList.remove('has-submenu');
      }
    }

    $updatePositions(parent, options = {}) {
      const menus = [parent].concat(Array.from(parent.querySelectorAll('ul')));
      for (const menu of menus) {
        if (this.animationDuration)
          menu.style.transition = `opacity ${this.animationDuration}ms ease-out`;
        else
          menu.style.transition = '';
        this.$updatePosition(menu, options);
      }
    }

    $updatePosition(menu, options = {}) {
      let left = options.left;
      let top  = options.top;
      const containerRect = this.$containerRect;
      const menuRect      = menu.getBoundingClientRect();

      if (options.anchor &&
          (left === undefined || top === undefined) &&
          menu == this.root) {
        const anchorRect = options.anchor.getBoundingClientRect();
        if (containerRect.bottom - anchorRect.bottom >= menuRect.height) {
          top = anchorRect.bottom;
          this.$marker.classList.add('top');
          this.$marker.classList.remove('bottom');
          this.$marker.style.top = `calc(${top}px - 0.4em)`;
        }
        else if (anchorRect.top - containerRect.top >= menuRect.height) {
          top = Math.max(0, anchorRect.top - menuRect.height);
          this.$marker.classList.add('bottom');
          this.$marker.classList.remove('top');
          this.$marker.style.top = `calc(${top}px + ${menuRect.height}px - 0.6em)`;
        }
        else {
          top = Math.max(0, containerRect.top - menuRect.height);
          this.$marker.classList.remove('bottom');
          this.$marker.classList.remove('top');
          this.$marker.style.top = `calc(${top}px + ${menuRect.height}px - 0.6em)`;
        }

        if (containerRect.right - anchorRect.left >= menuRect.width) {
          left = anchorRect.left;
          this.$marker.style.left = `calc(${left}px + 0.5em)`;
        }
        else if (anchorRect.left - containerRect.left >= menuRect.width) {
          left = Math.max(0, anchorRect.right - menuRect.width);
          this.$marker.style.left = `calc(${left}px + ${menuRect.width}px - 1.5em)`;
        }
        else {
          left = Math.max(0, containerRect.left - menuRect.width);
          this.$marker.style.left = `calc(${left}px + ${menuRect.width}px - 1.5em)`;
        }
      }

      let parentRect;
      if (menu.parentNode.localName == 'li') {
        parentRect = menu.parentNode.getBoundingClientRect();
        left = parentRect.right;
        top  = parentRect.top;
      }

      if (left === undefined)
        left = Math.max(0, (containerRect.width - menuRect.width) / 2);
      if (top === undefined)
        top = Math.max(0, (containerRect.height - menuRect.height) / 2);

      if (!options.anchor && menu == this.root) {
        // reposition to avoid the menu is opened below the cursor
        if (containerRect.bottom - top < menuRect.height) {
          top = top - menuRect.height;
        }
        if (containerRect.right - left < menuRect.width) {
          left = left - menuRect.width;
        }
      }

      const minMargin = 3;
      const overwrap  = 4;
      const firstTryLeft = Math.max(minMargin, Math.min(left - overwrap, containerRect.width - menuRect.width - minMargin));
      if (parentRect &&
          firstTryLeft < parentRect.right - overwrap &&
          containerRect.left < parentRect.left - menuRect.width + overwrap) {
        left = parentRect.left - menuRect.width + overwrap;
      }
      else {
        left = firstTryLeft;
      }
      menu.style.left = `${left}px`;

      top  = Math.max(minMargin, Math.min(top,  containerRect.height - menuRect.height - minMargin));
      if (menu == this.root && this.$marker.classList.contains('top'))
        menu.style.top = `calc(${top}px + 0.5em)`;
      else if (menu == this.root && this.$marker.classList.contains('bottom'))
        menu.style.top = `calc(${top}px - 0.5em)`;
      else
        menu.style.top = `${top}px`;
    }

    async close() {
      if (!this.opened)
        return;
      this.$tryCancelOpen();
      this.root.classList.remove('open');
      this.$screen.classList.remove('open');
      if (this.anchor) {
        this.anchor.classList.remove('open');
        this.$marker.classList.remove('open');
      }
      this.$mouseDownAfterOpen = false;
      this.$lastFocusedItem = null;
      this.$lastHoverItem = null;
      this.$mouseDownFired = false;
      this.anchor = null;
      this.canceller = null;
      return new Promise((resolve, _reject) => {
        this.closeTimeout = setTimeout(() => {
          delete this.closeTimeout;
          this.$onClosed();
          resolve();
        }, this.animationDuration);
      });
    }
    $onClosed() {
      const menus = [this.root].concat(Array.from(this.root.querySelectorAll('ul')));
      for (const menu of menus) {
        this.$updatePosition(menu, { left: 0, right: 0 });
      }
      this.root.parentNode.removeEventListener('mouseover', this.$onMouseOver);
      this.root.removeEventListener('transitionend', this.$onTransitionEnd);
      window.removeEventListener('contextmenu', this.$onContextMenu, { capture: true });
      window.removeEventListener('mousedown', this.$onMouseDown, { capture: true });
      window.removeEventListener('mouseup', this.$onMouseUp, { capture: true });
      window.removeEventListener('click', this.$onClick, { capture: true });
      window.removeEventListener('keydown', this.$onKeyDown, { capture: true });
      window.removeEventListener('keyup', this.$onKeyUp, { capture: true });
      window.removeEventListener('blur', this.$onBlur, { capture: true });
      this.onHidden();
    }

    get $containerRect() {
      const x      = 0;
      const y      = 0;
      const width  = window.innerWidth;
      const height = window.innerHeight;
      return {
        x, y, width, height,
        left:   x,
        top:    y,
        right:  width,
        bottom: height
      };
    }

    focusTo(item) {
      this.$lastFocusedItem = this.$lastHoverItem = item;
      this.$lastFocusedItem.focus();
      this.$lastFocusedItem.scrollIntoView({ block: 'nearest' });
    }

    $onBlur(event) {
      if (event.target == document)
        this.close();
    }

    $onMouseOver(event) {
      let item = this.$getEffectiveItem(event.target);
      if (this.delayedOpen && this.delayedOpen.item != item) {
        clearTimeout(this.delayedOpen.timer);
        this.delayedOpen = null;
      }
      if (item && item.delayedClose) {
        clearTimeout(item.delayedClose);
        item.delayedClose = null;
      }
      if (item && item.classList.contains('separator')) {
        this.$lastHoverItem = item;
        item = null;
      }
      if (!item) {
        if (this.$lastFocusedItem) {
          if (this.$lastFocusedItem.parentNode != this.root) {
            this.focusTo(this.$lastFocusedItem.parentNode.parentNode);
          }
          else {
            this.$lastFocusedItem.blur();
            this.$lastFocusedItem = null;
          }
        }
        this.$setHover(null);
        return;
      }

      this.$setHover(item);
      this.$closeOtherSubmenus(item);
      this.focusTo(item);

      this.delayedOpen = {
        item:  item,
        timer: setTimeout(() => {
          this.delayedOpen = null;
          this.$openSubmenuFor(item);
        }, this.subMenuOpenDelay)
      };
    }

    $setHover(item) {
      for (const item of this.root.querySelectorAll('li.hover')) {
        if (item != item)
          item.classList.remove('hover');
      }
      if (item)
        item.classList.add('hover');
    }

    $openSubmenuFor(item) {
      const items = MenuUI.$evaluateXPath(
        `ancestor-or-self::li[${MenuUI.$hasClass('has-submenu')}][not(${MenuUI.$hasClass('disabled')})]`,
        item
      );
      for (const item of MenuUI.$getArrayFromXPathResult(items)) {
        item.classList.add('open');
      }
    }

    $closeOtherSubmenus(item) {
      const items = MenuUI.$evaluateXPath(
        `preceding-sibling::li[${MenuUI.$hasClass('has-submenu')}] |
       following-sibling::li[${MenuUI.$hasClass('has-submenu')}] |
       preceding-sibling::li/descendant::li[${MenuUI.$hasClass('has-submenu')}] |
       following-sibling::li/descendant::li[${MenuUI.$hasClass('has-submenu')}]`,
        item
      );
      for (const item of MenuUI.$getArrayFromXPathResult(items)) {
        item.delayedClose = setTimeout(() => {
          item.classList.remove('open');
        }, this.subMenuCloseDelay);
      }
    }

    $onMouseDown(event) {
      event.stopImmediatePropagation();
      event.stopPropagation();
      event.preventDefault();
      this.$mouseDownAfterOpen = true;
      this.$mouseDownFired = true;
    }

    $getEffectiveItem(node) {
      const target = node.closest('li');
      let untransparentTarget = target && target.closest('ul');
      while (untransparentTarget) {
        if (parseFloat(window.getComputedStyle(untransparentTarget, null).opacity) < 1)
          return null;
        untransparentTarget = untransparentTarget.parentNode.closest('ul');
        if (untransparentTarget == document)
          break;
      }
      return target;
    }

    $onMouseUp(event) {
      if (!this.$mouseDownAfterOpen &&
        event.target.closest(`#${this.root.id}`))
        this.$onClick(event);
    }

    async $onClick(event) {
      event.stopImmediatePropagation();
      event.stopPropagation();
      event.preventDefault();

      const target = this.$getEffectiveItem(event.target);
      if (!target ||
        target.classList.contains('separator') ||
        target.classList.contains('has-submenu') ||
        target.classList.contains('disabled')) {
        if (this.$mouseDownFired && // ignore "click" event triggered by a mousedown fired before the menu is opened (like long-press)
            !event.target.closest(`#${this.root.id}`))
          return this.close();
        return;
      }

      this.onCommand(target, event);
    }

    $getNextFocusedItemByAccesskey(key) {
      for (const attribute of ['access-key', 'sub-access-key']) {
        const current = this.$lastHoverItem || this.$lastFocusedItem || this.root.firstChild;
        const condition = `@data-${attribute}="${key.toLowerCase()}"`;
        const item = this.$getNextItem(current, condition);
        if (item)
          return item;
      }
      return null;
    }

    $incrementalSearchNextFocusedItem(key) {
      this.$incrementalSearchString += key.toLowerCase();
      const current = this.$lastHoverItem || this.$lastFocusedItem || this.root.firstChild;
      const condition = `starts-with(@data-lower-cased-text, "${this.$incrementalSearchString.replace(/"/g, '\\"')}")`;
      if (this.$isItemMatches(current, condition))
        return current;
      const item = this.$getNextItem(current, condition);
      return item;
    }

    $shouldSearchIncremental() {
      if (!this.incrementalSearch)
        return false;

      const last = this.$lastKeyInputAt;
      const now = this.$lastKeyInputAt = Date.now();
      if (last < 0)
        return true; // start

      if (now - last > this.incrementalSearchTimeout)
        this.$incrementalSearchString = '';

      return true; // continue
    }

    $onKeyDown(event) {
      switch (event.key) {
        case 'ArrowUp':
          event.stopPropagation();
          event.preventDefault();
          this.$advanceFocus(-1);
          break;

        case 'ArrowDown':
          event.stopPropagation();
          event.preventDefault();
          this.$advanceFocus(1);
          break;

        case 'ArrowRight':
          event.stopPropagation();
          event.preventDefault();
          this.$digIn();
          break;

        case 'ArrowLeft':
          event.stopPropagation();
          event.preventDefault();
          this.$digOut();
          break;

        case 'Home':
          event.stopPropagation();
          event.preventDefault();
          this.$advanceFocus(1, (
            this.$lastHoverItem && this.$lastHoverItem.parentNode ||
          this.$lastFocusedItem && this.$lastFocusedItem.parentNode ||
          this.root
          ).lastChild);
          break;

        case 'End':
          event.stopPropagation();
          event.preventDefault();
          this.$advanceFocus(-1, (
            this.$lastHoverItem && this.$lastHoverItem.parentNode ||
          this.$lastFocusedItem && this.$lastFocusedItem.parentNode ||
          this.root
          ).firstChild);
          break;

        case 'Enter': {
          event.stopPropagation();
          event.preventDefault();
          const targetItem = this.$lastHoverItem || this.$lastFocusedItem;
          if (targetItem) {
            if (targetItem.classList.contains('disabled'))
              this.close();
            else if (!targetItem.classList.contains('separator'))
              this.onCommand(targetItem, event);
          }
        }; break;

        case 'Escape': {
          event.stopPropagation();
          event.preventDefault();
          const targetItem = this.$lastHoverItem || this.$lastFocusedItem;
          if (!targetItem ||
            targetItem.parentNode == this.root)
            this.close();
          else
            this.$digOut();
        }; break;

        case 'BackSpace': {
          if (this.$shouldSearchIncremental()) {
            event.stopPropagation();
            event.preventDefault();
            this.$incrementalSearchString = this.$incrementalSearchString.slice(0, this.$incrementalSearchString.length - 1);
            const item = this.$incrementalSearchNextFocusedItem('');
            if (item) {
              this.focusTo(item);
              this.$setHover(null);
            }
          }
        }; break;

        default:
          if (event.key.length == 1) {
            if (this.$shouldSearchIncremental()) {
              event.stopPropagation();
              event.preventDefault();
              const item = this.$incrementalSearchNextFocusedItem(event.key);
              if (item) {
                this.focusTo(item);
                this.$setHover(null);
              }
              return;
            }

            const item = this.$getNextFocusedItemByAccesskey(event.key);
            if (item) {
              this.focusTo(item);
              this.$setHover(null);
              if (this.$getNextFocusedItemByAccesskey(event.key) == item &&
                  !item.classList.contains('disabled')) {
                if (item.querySelector('ul'))
                  this.$digIn();
                else
                  this.onCommand(item, event);
              }
            }
          }
          return;
      }
    }

    $onKeyUp(event) {
      switch (event.key) {
        case 'ArrowUp':
        case 'ArrowDown':
        case 'ArrowRight':
        case 'ArrowLeft':
        case 'Home':
        case 'End':
        case 'Enter':
        case 'Escape':
        case 'Bakcspace':
          event.stopPropagation();
          event.preventDefault();
          return;

        default:
          if (event.key.length == 1 &&
              (this.$shouldSearchIncremental() ||
               this.$getNextFocusedItemByAccesskey(event.key))) {
            event.stopPropagation();
            event.preventDefault();
          }
          return;
      }
    }

    $getPreviousItem(base, condition = '') {
      const extrcondition = condition ? `[${condition}]` : '' ;
      const item = (
        MenuUI.$evaluateXPath(
          `preceding-sibling::li[not(${MenuUI.$hasClass('separator')})]${extrcondition}[1]`,
          base,
          XPathResult.FIRST_ORDERED_NODE_TYPE
        ).singleNodeValue ||
        MenuUI.$evaluateXPath(
          `following-sibling::li[not(${MenuUI.$hasClass('separator')})]${extrcondition}[last()]`,
          base,
          XPathResult.FIRST_ORDERED_NODE_TYPE
        ).singleNodeValue ||
        MenuUI.$evaluateXPath(
          `self::li[not(${MenuUI.$hasClass('separator')})]${extrcondition}`,
          base,
          XPathResult.FIRST_ORDERED_NODE_TYPE
        ).singleNodeValue
      );
      if (window.getComputedStyle(item, null).display == 'none')
        return this.$getPreviousItem(item, condition);
      return item;
    }

    $getNextItem(base, condition = '') {
      const extrcondition = condition ? `[${condition}]` : '' ;
      const item = (
        MenuUI.$evaluateXPath(
          `following-sibling::li[not(${MenuUI.$hasClass('separator')})]${extrcondition}[1]`,
          base,
          XPathResult.FIRST_ORDERED_NODE_TYPE
        ).singleNodeValue ||
        MenuUI.$evaluateXPath(
          `preceding-sibling::li[not(${MenuUI.$hasClass('separator')})]${extrcondition}[last()]`,
          base,
          XPathResult.FIRST_ORDERED_NODE_TYPE
        ).singleNodeValue ||
        MenuUI.$evaluateXPath(
          `self::li[not(${MenuUI.$hasClass('separator')})]${extrcondition}`,
          base,
          XPathResult.FIRST_ORDERED_NODE_TYPE
        ).singleNodeValue
      );
      if (item && window.getComputedStyle(item, null).display == 'none')
        return this.$getNextItem(item, condition);
      return item;
    }

    $isItemMatches(base, condition = '') {
      const extrcondition = condition ? `[${condition}]` : '' ;
      return !!MenuUI.$evaluateXPath(
        `self::li[not(${MenuUI.$hasClass('separator')})]${extrcondition}`,
        base,
        XPathResult.FIRST_ORDERED_NODE_TYPE
      ).singleNodeValue;
    }

    $advanceFocus(direction, lastFocused = null) {
      lastFocused = lastFocused || this.$lastHoverItem || this.$lastFocusedItem;
      if (!lastFocused) {
        if (direction < 0)
          this.$lastFocusedItem = lastFocused = this.root.firstChild;
        else
          this.$lastFocusedItem = lastFocused = this.root.lastChild;
      }
      this.focusTo(direction < 0 ? this.$getPreviousItem(lastFocused) : this.$getNextItem(lastFocused));
      this.$setHover(null);
    }

    $digIn() {
      if (!this.$lastFocusedItem) {
        this.$advanceFocus(1, this.root.lastChild);
        return;
      }
      const submenu = this.$lastFocusedItem.querySelector('ul');
      if (!submenu || this.$lastFocusedItem.classList.contains('disabled'))
        return;
      this.$closeOtherSubmenus(this.$lastFocusedItem);
      this.$openSubmenuFor(this.$lastFocusedItem);
      this.$advanceFocus(1, submenu.lastChild);
    }

    $digOut() {
      const targetItem = this.$lastHoverItem || this.$lastFocusedItem;
      if (!targetItem ||
        targetItem.parentNode == this.root)
        return;
      this.$closeOtherSubmenus(targetItem);
      this.$lastFocusedItem = targetItem.parentNode.parentNode;
      this.$closeOtherSubmenus(this.$lastFocusedItem);
      this.$lastFocusedItem.classList.remove('open');
      this.focusTo(targetItem.parentNode.parentNode);
      this.$setHover(null);
    }

    $onTransitionEnd(event) {
      const hoverItems = this.root.querySelectorAll('li:hover');
      if (hoverItems.length == 0)
        return;
      const $lastHoverItem = hoverItems[hoverItems.length - 1];
      const item = this.$getEffectiveItem($lastHoverItem);
      if (!item)
        return;
      if (item.parentNode != event.target)
        return;
      this.$setHover(item);
      this.focusTo(item);
    }

    $onContextMenu(event) {
      event.stopImmediatePropagation();
      event.stopPropagation();
      event.preventDefault();
    }


    static $installStyles() {
      this.style = document.createElement('style');
      this.style.setAttribute('type', 'text/css');
      const common = `.${this.$commonClass}`;
      this.style.textContent = `
        ${common}.menu-ui,
        ${common}.menu-ui ul {
          background-color: var(--menu-ui-background-color);
          color: var(--menu-ui-text-color);
          cursor: default;
          margin: 0;
          max-height: calc(100% - 6px);
          max-width: calc(100% - 6px);
          opacity: 0;
          overflow: hidden; /* because scrollbars always trap mouse events even if it is invisible. See also: https://github.com/piroor/treestyletab/issues/2386 */
          padding: 0;
          pointer-events: none;
          position: fixed;
          z-index: 999999;
        }

        ${common}.menu-ui.open,
        ${common}.menu-ui.open li.open > ul {
          opacity: 1;
          overflow: auto;
          pointer-events: auto;
        }

        ${common}.menu-ui li {
          list-style: none;
          margin: 0;
          padding: 0;
          overflow: hidden;
          text-overflow: ellipsis;
          white-space: nowrap;
        }

        ${common}.menu-ui li:not(.separator):focus,
        ${common}.menu-ui li:not(.separator).open {
          background-color: var(--menu-ui-background-color-active);
          color: var(--menu-ui-text-color-active);
        }

        ${common}.menu-ui li.radio.checked::before,
        ${common}.menu-ui li.checkbox.checked::before {
          content: "✔";
          position: absolute;
          left: 0.25em;
        }

        ${common}.menu-ui li.separator {
          height: 0.5em;
          visibility: hidden;
          margin: 0;
          padding: 0;
        }

        ${common}.menu-ui li.has-submenu,
        ${common}.menu-ui.menu li.has-submenu {
          padding-right: 1em;
        }
        ${common}.menu-ui li.has-submenu::after {
          content: "❯";
          position: absolute;
          right: 0.25em;
          transform: scale(0.75);
        }

        ${common}.menu-ui .accesskey {
          text-decoration: underline;
        }

        ${common}.menu-ui-blocking-screen {
          display: none;
        }

        ${common}.menu-ui-blocking-screen.open {
          bottom: 0;
          display: block;
          left: 0;
          position: fixed;
          right: 0;
          top: 0;
          z-index: 899999;
        }

        ${common}.menu-ui.menu li:not(.separator):focus,
        ${common}.menu-ui.menu li:not(.separator).open {
          outline: none;
        }

        ${common}.menu-ui.panel li:not(.separator):focus ul li:not(:focus):not(.open),
        ${common}.menu-ui.panel li:not(.separator).open ul li:not(:focus):not(.open) {
          background-color: transparent;
          color: var(--menu-ui-text-color);
        }

        ${common}.menu-ui-marker {
          opacity: 0;
          pointer-events: none;
          position: fixed;
        }

        ${common}.menu-ui-marker.open {
          border: 0.5em solid transparent;
          content: "";
          height: 0;
          left: 0;
          opacity: 1;
          top: 0;
          width: 0;
          z-index: 999999;
        }

        ${common}.menu-ui-marker.top {
          border-bottom: 0.5em solid var(--menu-ui-background-color);
        }
        ${common}.menu-ui-marker.bottom {
          border-top: 0.5em solid var(--menu-ui-background-color);
        }

        ${common}.menu-ui li.disabled {
          opacity: 0.5;
        }

        ${common}.menu-ui li[data-icon] {
          background-position: left center;
          background-repeat: no-repeat;
          background-size: 16px;
        }

        /* panel-like appearance */
        ${common}.panel {
          --menu-ui-background-color: -moz-dialog;
          --menu-ui-text-color: -moz-dialogtext;
          --menu-ui-background-color-active: Highlight;
          --menu-ui-text-color-active: HighlightText;
        }
        ${common}.panel li.disabled {
          --menu-ui-background-color-active: InactiveCaptionText;
          --menu-ui-text-color-active: InactiveCaption;
        }
        ${common}.menu-ui.panel,
        ${common}.menu-ui.panel ul {
          box-shadow: 0.1em 0.1em 0.8em rgba(0, 0, 0, 0.65);
          padding: 0.25em 0;
        }

        ${common}.menu-ui.panel li {
          padding: 0.15em 1em;
        }


        /* Menu-like appearance */
        ${common}.menu {
          --menu-ui-background-color: Menu;
          --menu-ui-text-color: MenuText;
          --menu-ui-background-color-active: Highlight;
          --menu-ui-text-color-active: HighlightText;
        }
        ${common}.menu li.disabled {
          --menu-ui-background-color-active: InactiveCaptionText;
          --menu-ui-text-color-active: InactiveCaption;
        }
        ${common}.menu-ui.menu,
        ${common}.menu-ui.menu ul {
          border: 1px outset Menu;
          box-shadow: 0.1em 0.1em 0.5em rgba(0, 0, 0, 0.65);
          font: -moz-pull-down-menu;
        }

        ${common}.menu-ui.menu li {
          padding: 0.15em 0.5em 0.15em 1.5em;
        }

        ${common}.menu-ui.menu li.separator {
          border: 1px inset Menu;
          height: 0;
          margin: 0 0.5em;
          max-height: 0;
          opacity: 0.5;
          padding: 0;
          visibility: visible;
        }

        ${common}.menu-ui.panel li[data-icon] {
          --icon-size: 16px;
          padding-left: calc(var(--icon-size) + 0.7em);
        }

        ${common}.menu-ui.menu li[data-icon]:not([data-icon-color]),
        ${common}.menu-ui.panel li[data-icon]:not([data-icon-color]) {
          background-position: 0.5em center;
        }

        ${common}.menu-ui li[data-icon]:not([data-icon-color]) .icon {
          display: none;
        }

        ${common}.menu-ui li[data-icon][data-icon-color] .icon {
          display: inline-block;
          height: var(--icon-size);
          left: 0.5em;
          max-height: var(--icon-size);
          max-width: var(--icon-size);
          position: absolute;
          width: var(--icon-size);
        }
      `;
      document.head.appendChild(this.style);
    }

    static init() {
      MenuUI.$uniqueKey   = parseInt(Math.random() * Math.pow(2, 16));
      MenuUI.$commonClass = `menu-ui-${MenuUI.$uniqueKey}`;

      MenuUI.prototype.$uniqueKey   = MenuUI.$uniqueKey;
      MenuUI.prototype.$commonClass = MenuUI.$commonClass;

      MenuUI.$installStyles();

      window.MenuUI = MenuUI;
    }
  };

  MenuUI.init();
}
export default MenuUI;
