/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

class PolyfillDropdown {
  constructor(select) {
    this._select = select;
    this._currentValue = select.value;

    this._visible = false;
    this._scrollbarWidth = 0;

    this._options = [];
    this._onChangedHandler = [];

    this._polyfill = document.createElement('div');
    this._polyfill.classList.add('polyfillDropdown');
    this._polyfill.classList.add('hidden');

    for (let elem of this._select.getElementsByTagName('option')) {
      this._options.push(elem.value);

      if (!elem.hidden) {
        let option = document.createElement('div');
        option.classList.add('polyfillOption');
        option.tabIndex = -1;
        option.dataset.value = elem.value;
        document.l10n.setAttributes(option,
          elem.dataset.l10nId, JSON.parse(elem.dataset.l10nArgs || '{}'));
        option.addEventListener('click',
          this._onOptionClicked.bind(this, elem.value));
        this._polyfill.appendChild(option);
      }
    }

    document.body.appendChild(this._polyfill);

    let dropdownStyle = getComputedStyle(this._polyfill);
    this._defaultDropdownBorderWidth =
      parseInt(dropdownStyle.getPropertyValue('border-left-width'), 10) +
      parseInt(dropdownStyle.getPropertyValue('border-right-width'), 10);

    let optionStyle = getComputedStyle(this._polyfill.childNodes[0]);
    this._defaultOptionPaddingRight =
      parseInt(optionStyle.getPropertyValue('padding-right'), 10);

    this._select.addEventListener('click', this);
    this._select.addEventListener('change', this);
    this._select.addEventListener('blur', this);
    this._select.addEventListener('keydown', this);
  }

  get value() {
    return this._select.value;
  }

  set value(value) {
    this._currentValue = value;
    this._select.value = value;
  }

  _show() {
    if (this._visible) {
      return;
    }
    this._visible = true;
    this._updateOptionStatus();
    this._polyfill.classList.remove('hidden');
    this._resize();

    window.addEventListener('resize', this);
  }

  _hide() {
    if (!this._visible) {
      return;
    }
    window.removeEventListener('resize', this);
    this._polyfill.classList.add('hidden');
    this._visible = false;

    if (this._select.value !== this._currentValue) {
      this._triggerChangeEvent();
    }
  }

  _updateOptionStatus() {
    let currentValue = this._select.value;
    for (let elem of this._polyfill.childNodes) {
      elem.classList.toggle('toggled', elem.dataset.value === currentValue);
    };
  }

  _resize() {
    let rect = this._select.getBoundingClientRect();
    this._polyfill.style.left = rect.left + 'px';
    this._polyfill.style.top = rect.bottom + 'px';

    let scrollbarWidth = this._polyfill.offsetWidth -
      this._polyfill.clientWidth - this._defaultDropdownBorderWidth;

    if (scrollbarWidth != this._scrollbarWidth) {
      this._scrollbarWidth = scrollbarWidth;
      Array.from(this._polyfill.getElementsByClassName('polyfillOption'))
        .forEach(option => {
          if (scrollbarWidth > 0) {
            option.style.paddingRight =
              (scrollbarWidth + this._defaultOptionPaddingRight) + 'px';
          } else {
            option.style.paddingRight = '';
          }
        });
    }
  }

  _onOptionClicked(value) {
    this._select.value = value;
    this._hide();
  }

  _triggerChangeEvent() {
    this._currentValue = this._select.value;

    let event = {
      type: 'change',
      target: this._select
    };

    this._onChangedHandler.forEach(handler => {
      if (typeof handler === 'function') {
        handler.call(this._select, event);
      } else if (typeof handler === 'object' &&
                 typeof handler.handleEvent === 'function') {
        handler.handleEvent(event);
      }
    });
  }

  addEventListener(type, listener, useCapture) {
    if (type == 'change') {
      this._onChangedHandler.push(listener);
    } else {
      this._select.addEventListener(type, listener, useCapture);
    }
  }

  querySelectorAll(selectors) {
    return this._select.querySelectorAll(selectors);
  }

  handleEvent(evt) {
    let handled = false;

    switch(evt.type) {
      case 'click':
        if (this._visible) {
          this._hide();
        } else {
          this._show();
        }
        handled = true;
        break;
      case 'change':
        if (this._visible) {
          this._updateOptionStatus();
        } else if (this._select.value !== this._currentValue) {
          this._triggerChangeEvent();
        }
        break;
      case 'blur':
        setTimeout(() => {
          if (!this._polyfill.contains(document.activeElement)) {
            this._hide();
          }
        });
        break;
      case 'resize':
        this._resize();
        break;
      case 'keydown':
        let isMac = navigator.platform.startsWith('Mac');

        if (this._visible) {
          let index;

          switch(evt.keyCode) {
            case KeyEvent.DOM_VK_ESCAPE:
            case KeyEvent.DOM_VK_RETURN:
              this._hide();
              handled = true;
              break;
            case KeyEvent.DOM_VK_DOWN:
              if (isMac) {
                index = this._options.indexOf(this._select.value);
                if (index < this._options.length - 1) {
                  this._select.value = this._options[index + 1];
                  this._updateOptionStatus();
                }
                handled = true;
              }
              break;
            case KeyEvent.DOM_VK_UP:
              if (isMac) {
                index = this._options.indexOf(this._select.value);
                if (index > 0) {
                  this._select.value = this._options[index - 1];
                  this._updateOptionStatus();
                }
                handled = true;
              }
              break;
          }
        } else if (document.activeElement == this._select) {
          switch(evt.keyCode) {
            case KeyEvent.DOM_VK_SPACE:
              this._show();
              handled = true;
              break;
            case KeyEvent.DOM_VK_DOWN:
            case KeyEvent.DOM_VK_UP:
              if (isMac) {
                this._show();
                handled = true;
              }
              break;
          }
        }

        break;
    }

    if (handled) {
      evt.stopPropagation();
      evt.preventDefault();
    }
  }
}
