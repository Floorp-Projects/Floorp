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
var l10n = require('../util/l10n');

/**
 * Record how much help the user wants from the tooltip
 */
var Eagerness = {
  NEVER: 1,
  SOMETIMES: 2,
  ALWAYS: 3
};

/**
 * Export the eagerHelper setting
 */
exports.items = [
  {
    item: 'setting',
    name: 'eagerHelper',
    type: {
      name: 'selection',
      lookup: [
        { name: 'never', value: Eagerness.NEVER },
        { name: 'sometimes', value: Eagerness.SOMETIMES },
        { name: 'always', value: Eagerness.ALWAYS }
      ]
    },
    defaultValue: Eagerness.SOMETIMES,
    description: l10n.lookup('eagerHelperDesc'),
    ignoreTypeDifference: true
  }
];

/**
 * FocusManager solves the problem of tracking focus among a set of nodes.
 * The specific problem we are solving is when the hint element must be visible
 * if either the command line or any of the inputs in the hint element has the
 * focus, and invisible at other times, without hiding and showing the hint
 * element even briefly as the focus changes between them.
 * It does this simply by postponing the hide events by 250ms to see if
 * something else takes focus.
 */
function FocusManager(document, settings) {
  if (document == null) {
    throw new Error('document == null');
  }

  this.document = document;
  this.settings = settings;
  this.debug = false;
  this.blurDelay = 150;
  this.window = this.document.defaultView;

  this._blurDelayTimeout = null; // Result of setTimeout in delaying a blur
  this._monitoredElements = [];  // See addMonitoredElement()

  this._isError = false;
  this._hasFocus = false;
  this._helpRequested = false;
  this._recentOutput = false;

  this.onVisibilityChange = util.createEvent('FocusManager.onVisibilityChange');

  this._focused = this._focused.bind(this);
  if (this.document.addEventListener) {
    this.document.addEventListener('focus', this._focused, true);
  }

  var eagerHelper = this.settings.get('eagerHelper');
  eagerHelper.onChange.add(this._eagerHelperChanged, this);

  this.isTooltipVisible = undefined;
  this.isOutputVisible = undefined;
  this._checkShow();
}

/**
 * Avoid memory leaks
 */
FocusManager.prototype.destroy = function() {
  var eagerHelper = this.settings.get('eagerHelper');
  eagerHelper.onChange.remove(this._eagerHelperChanged, this);

  this.document.removeEventListener('focus', this._focused, true);

  for (var i = 0; i < this._monitoredElements.length; i++) {
    var monitor = this._monitoredElements[i];
    console.error('Hanging monitored element: ', monitor.element);

    monitor.element.removeEventListener('focus', monitor.onFocus, true);
    monitor.element.removeEventListener('blur', monitor.onBlur, true);
  }

  if (this._blurDelayTimeout) {
    this.window.clearTimeout(this._blurDelayTimeout);
    this._blurDelayTimeout = null;
  }

  this._focused = undefined;
  this.document = undefined;
  this.settings = undefined;
  this.window = undefined;
};

/**
 * The easy way to include an element in the set of things that are part of the
 * aggregate focus. Using [add|remove]MonitoredElement() is a simpler way of
 * option than calling report[Focus|Blur]()
 * @param element The element on which to track focus|blur events
 * @param where Optional source string for debugging only
 */
FocusManager.prototype.addMonitoredElement = function(element, where) {
  if (this.debug) {
    console.log('FocusManager.addMonitoredElement(' + (where || 'unknown') + ')');
  }

  var monitor = {
    element: element,
    where: where,
    onFocus: function() { this._reportFocus(where); }.bind(this),
    onBlur: function() { this._reportBlur(where); }.bind(this)
  };

  element.addEventListener('focus', monitor.onFocus, true);
  element.addEventListener('blur', monitor.onBlur, true);

  if (this.document.activeElement === element) {
    this._reportFocus(where);
  }

  this._monitoredElements.push(monitor);
};

/**
 * Undo the effects of addMonitoredElement()
 * @param element The element to stop tracking
 * @param where Optional source string for debugging only
 */
FocusManager.prototype.removeMonitoredElement = function(element, where) {
  if (this.debug) {
    console.log('FocusManager.removeMonitoredElement(' + (where || 'unknown') + ')');
  }

  this._monitoredElements = this._monitoredElements.filter(function(monitor) {
    if (monitor.element === element) {
      element.removeEventListener('focus', monitor.onFocus, true);
      element.removeEventListener('blur', monitor.onBlur, true);
      return false;
    }
    return true;
  });
};

/**
 * Monitor for new command executions
 */
FocusManager.prototype.updatePosition = function(dimensions) {
  var ev = {
    tooltipVisible: this.isTooltipVisible,
    outputVisible: this.isOutputVisible,
    dimensions: dimensions
  };
  this.onVisibilityChange(ev);
};

/**
 * Monitor for new command executions
 */
FocusManager.prototype.outputted = function() {
  this._recentOutput = true;
  this._helpRequested = false;
  this._checkShow();
};

/**
 * We take a focus event anywhere to be an indication that we might be about
 * to lose focus
 */
FocusManager.prototype._focused = function() {
  this._reportBlur('document');
};

/**
 * Some component has received a 'focus' event. This sets the internal status
 * straight away and informs the listeners
 * @param where Optional source string for debugging only
 */
FocusManager.prototype._reportFocus = function(where) {
  if (this.debug) {
    console.log('FocusManager._reportFocus(' + (where || 'unknown') + ')');
  }

  if (this._blurDelayTimeout) {
    if (this.debug) {
      console.log('FocusManager.cancelBlur');
    }
    this.window.clearTimeout(this._blurDelayTimeout);
    this._blurDelayTimeout = null;
  }

  if (!this._hasFocus) {
    this._hasFocus = true;
  }
  this._checkShow();
};

/**
 * Some component has received a 'blur' event. This waits for a while to see if
 * we are going to get any subsequent 'focus' events and then sets the internal
 * status and informs the listeners
 * @param where Optional source string for debugging only
 */
FocusManager.prototype._reportBlur = function(where) {
  if (this.debug) {
    console.log('FocusManager._reportBlur(' + where + ')');
  }

  if (this._hasFocus) {
    if (this._blurDelayTimeout) {
      if (this.debug) {
        console.log('FocusManager.blurPending');
      }
      return;
    }

    this._blurDelayTimeout = this.window.setTimeout(function() {
      if (this.debug) {
        console.log('FocusManager.blur');
      }
      this._hasFocus = false;
      this._checkShow();
      this._blurDelayTimeout = null;
    }.bind(this), this.blurDelay);
  }
};

/**
 * The setting has changed
 */
FocusManager.prototype._eagerHelperChanged = function() {
  this._checkShow();
};

/**
 * The terminal tells us about keyboard events so we can decide to delay
 * showing the tooltip element
 */
FocusManager.prototype.onInputChange = function() {
  this._recentOutput = false;
  this._checkShow();
};

/**
 * Generally called for something like a F1 key press, when the user explicitly
 * wants help
 */
FocusManager.prototype.helpRequest = function() {
  if (this.debug) {
    console.log('FocusManager.helpRequest');
  }

  this._helpRequested = true;
  this._recentOutput = false;
  this._checkShow();
};

/**
 * Generally called for something like a ESC key press, when the user explicitly
 * wants to get rid of the help
 */
FocusManager.prototype.removeHelp = function() {
  if (this.debug) {
    console.log('FocusManager.removeHelp');
  }

  this._importantFieldFlag = false;
  this._isError = false;
  this._helpRequested = false;
  this._recentOutput = false;
  this._checkShow();
};

/**
 * Set to true whenever a field thinks it's output is important
 */
FocusManager.prototype.setImportantFieldFlag = function(flag) {
  if (this.debug) {
    console.log('FocusManager.setImportantFieldFlag', flag);
  }
  this._importantFieldFlag = flag;
  this._checkShow();
};

/**
 * Set to true whenever a field thinks it's output is important
 */
FocusManager.prototype.setError = function(isError) {
  if (this.debug) {
    console.log('FocusManager._isError', isError);
  }
  this._isError = isError;
  this._checkShow();
};

/**
 * Helper to compare the current showing state with the value calculated by
 * _shouldShow() and take appropriate action
 */
FocusManager.prototype._checkShow = function() {
  var fire = false;
  var ev = {
    tooltipVisible: this.isTooltipVisible,
    outputVisible: this.isOutputVisible
  };

  var showTooltip = this._shouldShowTooltip();
  if (this.isTooltipVisible !== showTooltip.visible) {
    ev.tooltipVisible = this.isTooltipVisible = showTooltip.visible;
    fire = true;
  }

  var showOutput = this._shouldShowOutput();
  if (this.isOutputVisible !== showOutput.visible) {
    ev.outputVisible = this.isOutputVisible = showOutput.visible;
    fire = true;
  }

  if (fire) {
    if (this.debug) {
      console.log('FocusManager.onVisibilityChange', ev);
    }
    this.onVisibilityChange(ev);
  }
};

/**
 * Calculate if we should be showing or hidden taking into account all the
 * available inputs
 */
FocusManager.prototype._shouldShowTooltip = function() {
  var eagerHelper = this.settings.get('eagerHelper');
  if (eagerHelper.value === Eagerness.NEVER) {
    return { visible: false, reason: 'eagerHelperNever' };
  }

  if (eagerHelper.value === Eagerness.ALWAYS) {
    return { visible: true, reason: 'eagerHelperAlways' };
  }

  if (!this._hasFocus) {
    return { visible: false, reason: 'notHasFocus' };
  }

  if (this._isError) {
    return { visible: true, reason: 'isError' };
  }

  if (this._helpRequested) {
    return { visible: true, reason: 'helpRequested' };
  }

  if (this._importantFieldFlag) {
    return { visible: true, reason: 'importantFieldFlag' };
  }

  return { visible: false, reason: 'default' };
};

/**
 * Calculate if we should be showing or hidden taking into account all the
 * available inputs
 */
FocusManager.prototype._shouldShowOutput = function() {
  if (!this._hasFocus) {
    return { visible: false, reason: 'notHasFocus' };
  }

  if (this._recentOutput) {
    return { visible: true, reason: 'recentOutput' };
  }

  return { visible: false, reason: 'default' };
};

exports.FocusManager = FocusManager;
