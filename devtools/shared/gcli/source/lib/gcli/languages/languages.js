/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

'use strict';

var util = require('../util/util');

var RESOLVED = Promise.resolve(true);

/**
 * This is the base implementation for all languages
 */
var baseLanguage = {
  item: 'language',
  name: undefined,

  constructor: function(terminal) {
  },

  destroy: function() {
  },

  updateHints: function() {
    util.clearElement(this.terminal.tooltipElement);
  },

  description: '',
  message: '',
  caretMoved: function() {},

  handleUpArrow: function() {
    return Promise.resolve(false);
  },

  handleDownArrow: function() {
    return Promise.resolve(false);
  },

  handleTab: function() {
    return this.terminal.unsetChoice().then(function() {
      return RESOLVED;
    }, util.errorHandler);
  },

  handleInput: function(input) {
    if (input === ':') {
      return this.terminal.setInput('').then(() => {
        return this.terminal.pushLanguage('commands');
      });
    }

    return this.terminal.unsetChoice().then(function() {
      return RESOLVED;
    }, util.errorHandler);
  },

  handleReturn: function(input) {
    var rowoutEle = this.document.createElement('pre');
    rowoutEle.classList.add('gcli-row-out');
    rowoutEle.classList.add('gcli-row-script');
    rowoutEle.setAttribute('aria-live', 'assertive');

    return this.exec(input).then(line => {
      rowoutEle.innerHTML = line;

      this.terminal.addElement(rowoutEle);
      this.terminal.scrollToBottom();

      this.focusManager.outputted();

      this.terminal.unsetChoice().catch(util.errorHandler);
      this.terminal.inputElement.value = '';
    });
  },

  setCursor: function(cursor) {
    this.terminal.inputElement.selectionStart = cursor.start;
    this.terminal.inputElement.selectionEnd = cursor.end;
  },

  getCompleterTemplateData: function() {
    return Promise.resolve({
      statusMarkup: [
        {
          string: this.terminal.inputElement.value.replace(/ /g, '\u00a0'), // i.e. &nbsp;
          className: 'gcli-in-valid'
        }
      ],
      unclosedJs: false,
      directTabText: '',
      arrowTabText: '',
      emptyParameters: ''
    });
  },

  showIntro: function() {
  },

  exec: function(input) {
    throw new Error('Missing implementation of handleReturn() or exec() ' + this.name);
  }
};

/**
 * A manager for the registered Languages
 */
function Languages() {
  // This is where we cache the languages that we know about
  this._registered = {};
}

/**
 * Add a new language to the cache
 */
Languages.prototype.add = function(language) {
  this._registered[language.name] = language;
};

/**
 * Remove an existing language from the cache
 */
Languages.prototype.remove = function(language) {
  var name = typeof language === 'string' ? language : language.name;
  delete this._registered[name];
};

/**
 * Get access to the list of known languages
 */
Languages.prototype.getAll = function() {
  return Object.keys(this._registered).map(name => {
    return this._registered[name];
  });
};

/**
 * Find a previously registered language
 */
Languages.prototype.createLanguage = function(name, terminal) {
  if (name == null) {
    name = Object.keys(this._registered)[0];
  }

  var language = (typeof name === 'string') ? this._registered[name] : name;
  if (!language) {
    console.error('Known languages: ' + Object.keys(this._registered).join(', '));
    throw new Error('Unknown language: \'' + name + '\'');
  }

  // clone 'type'
  var newInstance = {};
  util.copyProperties(baseLanguage, newInstance);
  util.copyProperties(language, newInstance);

  if (typeof newInstance.constructor === 'function') {
    var reply = newInstance.constructor(terminal);
    return Promise.resolve(reply).then(function() {
      return newInstance;
    });
  }
  else {
    return Promise.resolve(newInstance);
  }
};

exports.Languages = Languages;
