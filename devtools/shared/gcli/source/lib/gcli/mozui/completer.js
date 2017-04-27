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
var host = require('../util/host');
var domtemplate = require('../util/domtemplate');

var completerHtml =
  '<description\n' +
  '    xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul">\n' +
  '  <loop foreach="member in ${statusMarkup}">\n' +
  '    <label class="${member.className}" value="${member.string}"></label>\n' +
  '  </loop>\n' +
  '  <label class="gcli-in-ontab" value="${directTabText}"/>\n' +
  '  <label class="gcli-in-todo" foreach="param in ${emptyParameters}" value="${param}"/>\n' +
  '  <label class="gcli-in-ontab" value="${arrowTabText}"/>\n' +
  '  <label class="gcli-in-closebrace" if="${unclosedJs}" value="}"/>\n' +
  '</description>\n';

/**
 * Completer is an 'input-like' element that sits  an input element annotating
 * it with visual goodness.
 * @param components Object that links to other UI components. GCLI provided:
 * - requisition: A GCLI Requisition object whose state is monitored
 * - element: Element to use as root
 * - autoResize: (default=false): Should we attempt to sync the dimensions of
 *   the complete element with the input element.
 */
function Completer(components) {
  this.requisition = components.requisition;
  this.input = { typed: '', cursor: { start: 0, end: 0 } };
  this.choice = 0;

  this.element = components.element;
  this.element.classList.add('gcli-in-complete');
  this.element.setAttribute('tabindex', '-1');
  this.element.setAttribute('aria-live', 'polite');

  this.document = this.element.ownerDocument;

  this.inputter = components.inputter;

  this.inputter.onInputChange.add(this.update, this);
  this.inputter.onAssignmentChange.add(this.update, this);
  this.inputter.onChoiceChange.add(this.update, this);

  this.autoResize = components.autoResize;
  if (this.autoResize) {
    this.inputter.onResize.add(this.resized, this);

    var dimensions = this.inputter.getDimensions();
    if (dimensions) {
      this.resized(dimensions);
    }
  }

  this.template = host.toDom(this.document, completerHtml);
  // We want the spans to line up without the spaces in the template
  util.removeWhitespace(this.template, true);

  this.update();
}

/**
 * Avoid memory leaks
 */
Completer.prototype.destroy = function() {
  this.inputter.onInputChange.remove(this.update, this);
  this.inputter.onAssignmentChange.remove(this.update, this);
  this.inputter.onChoiceChange.remove(this.update, this);

  if (this.autoResize) {
    this.inputter.onResize.remove(this.resized, this);
  }

  this.document = undefined;
  this.element = undefined;
  this.template = undefined;
  this.inputter = undefined;
};

/**
 * Ensure that the completion element is the same size and the inputter element
 */
Completer.prototype.resized = function(ev) {
  this.element.style.top = ev.top + 'px';
  this.element.style.height = ev.height + 'px';
  this.element.style.lineHeight = ev.height + 'px';
  this.element.style.left = ev.left + 'px';
  this.element.style.width = ev.width + 'px';
};

/**
 * Bring the completion element up to date with what the requisition says
 */
Completer.prototype.update = function(ev) {
  this.choice = (ev && ev.choice != null) ? ev.choice : 0;

  this._getCompleterTemplateData().then(data => {
    if (this.template == null) {
      return; // destroy() has been called
    }

    var template = this.template.cloneNode(true);
    domtemplate.template(template, data, { stack: 'completer.html' });

    util.clearElement(this.element);
    while (template.hasChildNodes()) {
      this.element.appendChild(template.firstChild);
    }
  });
};

/**
 * Calculate the properties required by the template process for completer.html
 */
Completer.prototype._getCompleterTemplateData = function() {
  var input = this.inputter.getInputState();
  var start = input.cursor.start;

  return this.requisition.getStateData(start, this.choice).then(function(data) {
    // Calculate the statusMarkup required to show wavy lines underneath the
    // input text (like that of an inline spell-checker) which used by the
    // template process for completer.html
    // i.e. s/space/&nbsp/g in the string (for HTML display) and status to an
    // appropriate class name (i.e. lower cased, prefixed with gcli-in-)
    data.statusMarkup.forEach(function(member) {
      member.string = member.string.replace(/ /g, '\u00a0'); // i.e. &nbsp;
      member.className = 'gcli-in-' + member.status.toString().toLowerCase();
    }, this);

    return data;
  });
};

exports.Completer = Completer;
