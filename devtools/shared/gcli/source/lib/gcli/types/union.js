/*
 * Copyright 2014, Mozilla Foundation and contributors
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

var l10n = require('../util/l10n');
var Conversion = require('./types').Conversion;
var Status = require('./types').Status;

exports.items = [
  {
    // The union type allows for a combination of different parameter types.
    item: 'type',
    name: 'union',
    hasPredictions: true,

    constructor: function() {
      // Get the properties of the type. Later types in the list should always
      // be more general, so 'catch all' types like string must be last
      this.alternatives = this.alternatives.map(typeData => {
        return this.types.createType(typeData);
      });
    },

    getSpec: function(command, param) {
      var spec = { name: 'union', alternatives: [] };
      this.alternatives.forEach(type => {
        spec.alternatives.push(type.getSpec(command, param));
      });
      return spec;
    },

    stringify: function(value, context) {
      if (value == null) {
        return '';
      }

      var type = this.alternatives.find(function(typeData) {
        return typeData.name === value.type;
      });

      return type.stringify(value[value.type], context);
    },

    parse: function(arg, context) {
      var conversionPromises = this.alternatives.map(type => {
        return type.parse(arg, context);
      });

      return Promise.all(conversionPromises).then(conversions => {
        // Find a list of the predictions made by any conversion
        var predictionPromises = conversions.map(conversion => {
          return conversion.getPredictions(context);
        });

        return Promise.all(predictionPromises).then(allPredictions => {
          // Take one prediction from each set of predictions, ignoring
          // duplicates, until we've got up to Conversion.maxPredictions
          var maxIndex = allPredictions.reduce((prev, prediction) => {
            return Math.max(prev, prediction.length);
          }, 0);
          var predictions = [];

          indexLoop:
          for (var index = 0; index < maxIndex; index++) {
            for (var p = 0; p <= allPredictions.length; p++) {
              if (predictions.length >= Conversion.maxPredictions) {
                break indexLoop;
              }

              if (allPredictions[p] != null) {
                var prediction = allPredictions[p][index];
                if (prediction != null && predictions.indexOf(prediction) === -1) {
                  predictions.push(prediction);
                }
              }
            }
          }

          var bestStatus = Status.ERROR;
          var value;
          for (var i = 0; i < conversions.length; i++) {
            var conversion = conversions[i];
            var thisStatus = conversion.getStatus(arg);
            if (thisStatus < bestStatus) {
              bestStatus = thisStatus;
            }
            if (bestStatus === Status.VALID) {
              var type = this.alternatives[i].name;
              value = { type: type };
              value[type] = conversion.value;
              break;
            }
          }

          var msg = (bestStatus === Status.VALID) ?
                    '' :
                    l10n.lookupFormat('typesSelectionNomatch', [ arg.text ]);
          return new Conversion(value, arg, bestStatus, msg, predictions);
        });
      });
    },
  }
];
