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
var Field = require('./fields').Field;

/**
 * A field that works with delegate types by delaying resolution until that
 * last possible time
 */
function DelegateField(type, options) {
  Field.call(this, type, options);
  this.options = options;

  this.element = util.createElement(this.document, 'div');
  this.update();

  this.onFieldChange = util.createEvent('DelegateField.onFieldChange');
}

DelegateField.prototype = Object.create(Field.prototype);

DelegateField.prototype.update = function() {
  var subtype = this.type.getType(this.options.requisition.executionContext);
  if (typeof subtype.parse !== 'function') {
    subtype = this.options.requisition.system.types.createType(subtype);
  }

  // It's not clear that we can compare subtypes in this way.
  // Perhaps we need a type.equals(...) function
  if (subtype === this.subtype) {
    return;
  }

  if (this.field) {
    this.field.destroy();
  }

  this.subtype = subtype;
  var fields = this.options.requisition.system.fields;
  this.field = fields.get(subtype, this.options);

  util.clearElement(this.element);
  this.element.appendChild(this.field.element);
};

DelegateField.claim = function(type, context) {
  return type.isDelegate ? Field.MATCH : Field.NO_MATCH;
};

DelegateField.prototype.destroy = function() {
  this.element = undefined;
  this.options = undefined;
  if (this.field) {
    this.field.destroy();
  }
  this.subtype = undefined;
  Field.prototype.destroy.call(this);
};

DelegateField.prototype.setConversion = function(conversion) {
  this.field.setConversion(conversion);
};

DelegateField.prototype.getConversion = function() {
  return this.field.getConversion();
};

Object.defineProperty(DelegateField.prototype, 'isImportant', {
  get: function() {
    return this.field.isImportant;
  },
  enumerable: true
});

/**
 * Exported items
 */
exports.items = [
  DelegateField
];
