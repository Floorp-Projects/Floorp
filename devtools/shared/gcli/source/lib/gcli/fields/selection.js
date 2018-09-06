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
var Menu = require('../ui/menu').Menu;

var Argument = require('../types/types').Argument;
var Conversion = require('../types/types').Conversion;
var Field = require('./fields').Field;

/**
 * A field that allows selection of one of a number of options
 */
function SelectionField(type, options) {
  Field.call(this, type, options);

  this.arg = new Argument();

  this.menu = new Menu({
    document: this.document,
    maxPredictions: Conversion.maxPredictions
  });
  this.element = this.menu.element;

  this.onFieldChange = util.createEvent('SelectionField.onFieldChange');

  // i.e. Register this.onItemClick as the default action for a menu click
  this.menu.onItemClick.add(this.itemClicked, this);
}

SelectionField.prototype = Object.create(Field.prototype);

SelectionField.claim = function(type, context) {
  if (context == null) {
    return Field.NO_MATCH;
  }
  return type.getType(context).hasPredictions ? Field.DEFAULT : Field.NO_MATCH;
};

SelectionField.prototype.destroy = function() {
  this.menu.onItemClick.remove(this.itemClicked, this);
  this.menu.destroy();
  this.menu = undefined;
  this.element = undefined;
  Field.prototype.destroy.call(this);
};

SelectionField.prototype.setConversion = function(conversion) {
  this.arg = conversion.arg;
  this.setMessage(conversion.message);

  var context = this.requisition.executionContext;
  conversion.getPredictions(context).then(predictions => {
    var items = predictions.map(function(prediction) {
      // If the prediction value is an 'item' (that is an object with a name and
      // description) then use that, otherwise use the prediction itself, because
      // at least that has a name.
      return prediction.value && prediction.value.description ?
          prediction.value :
          prediction;
    }, this);
    if (this.menu != null) {
      this.menu.show(items, conversion.arg.text);
    }
  }).catch(util.errorHandler);
};

SelectionField.prototype.itemClicked = function(ev) {
  var arg = new Argument(ev.name, '', ' ');
  var context = this.requisition.executionContext;

  this.type.parse(arg, context).then(conversion => {
    this.onFieldChange({ conversion: conversion });
    this.setMessage(conversion.message);
  }).catch(util.errorHandler);
};

SelectionField.prototype.getConversion = function() {
  // This tweaks the prefix/suffix of the argument to fit
  this.arg = this.arg.beget({ text: this.input.value });
  return this.type.parse(this.arg, this.requisition.executionContext);
};

/**
 * Allow the terminal to use RETURN to chose the current menu item when
 * it can't execute the command line
 * @return true if an item was 'clicked', false otherwise
 */
SelectionField.prototype.selectChoice = function() {
  var selected = this.menu.selected;
  if (selected == null) {
    return false;
  }

  this.itemClicked({ name: selected });
  return true;
};

Object.defineProperty(SelectionField.prototype, 'isImportant', {
  get: function() {
    return this.type.name !== 'command';
  },
  enumerable: true
});

/**
 * Allow registration and de-registration.
 */
exports.items = [ SelectionField ];
