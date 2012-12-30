/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

this.EXPORTED_SYMBOLS = ['ActivitiesServiceFilter'];

this.ActivitiesServiceFilter = {
  match: function(aValues, aFilters) {

    function matchValue(aValue, aFilter, aFilterObj) {
      if (aFilter !== null) {
        // Custom functions for the different types.
        switch (typeof(aFilter)) {
        case 'boolean':
          return aValue === aFilter;

        case 'number':
          return Number(aValue) === aFilter;

        case 'string':
          return String(aValue) === aFilter;

        default: // not supported
          return false;
        }
      }

      // Regexp.
      if (('regexp' in aFilterObj)) {
        var regexp = String(aFilterObj.regexp);

        if (regexp[0] != "/")
          return false;

        var pos = regexp.lastIndexOf("/");
        if (pos == 0)
          return false;

        var re = new RegExp(regexp.substring(1, pos), regexp.substr(pos + 1));
        return re.test(aValue);
      }

      // Validation of the min/Max.
      if (('min' in aFilterObj) || ('max' in aFilterObj)) {
        // Min value.
        if (('min' in aFilterObj) &&
            aFilterObj.min > aValue) {
          return false;
        }

        // Max value.
        if (('max' in aFilterObj) &&
            aFilterObj.max < aValue) {
          return false;
        }
      }

      return true;
    }

    // this function returns true if the value matches with the filter object
    function matchObject(aValue, aFilterObj) {

      // Let's consider anything an array.
      let filters = ('value' in aFilterObj)
                      ? (Array.isArray(aFilterObj.value)
                          ? aFilterObj.value
                          : [aFilterObj.value])
                      : [ null ];
      let values  = Array.isArray(aValue) ? aValue : [aValue];

      for (var filterId = 0; filterId < filters.length; ++filterId) {
        for (var valueId = 0; valueId < values.length; ++valueId) {
          if (matchValue(values[valueId], filters[filterId], aFilterObj)) {
            return true;
          }
        }
      }

      return false;
    }

    // Creation of a filter map useful to know what has been
    // matched and what is not.
    let filtersMap = {}
    for (let filter in aFilters) {
      // Convert this filter in an object if needed
      let filterObj = aFilters[filter];

      if (Array.isArray(filterObj) || typeof(filterObj) !== 'object') {
        filterObj = {
          required: false,
          value: filterObj
        }
      }

      filtersMap[filter] = { filter: filterObj,
                             found:  false };
    }

    // For any incoming property.
    for (let prop in aValues) {
      // If this is unknown for the app, let's continue.
      if (!(prop in filtersMap)) {
        continue;
      }

      // Otherwise, let's check the value against the filter.
      if (!matchObject(aValues[prop], filtersMap[prop].filter)) {
        return false;
      }

      filtersMap[prop].found = true;
    }

    // Required filters:
    for (let filter in filtersMap) {
      if (filtersMap[filter].filter.required && !filtersMap[filter].found) {
        return false;
      }
    }

    return true;
  }
}
