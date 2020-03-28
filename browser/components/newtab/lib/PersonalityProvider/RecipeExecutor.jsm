/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// We load this into a worker using importScripts, and in tests using import.
// We use var to avoid name collision errors.
// eslint-disable-next-line no-var
var EXPORTED_SYMBOLS = ["RecipeExecutor"];

/**
 * RecipeExecutor is the core feature engineering pipeline for the in-browser
 * personalization work. These pipelines are called "recipes". A recipe is an
 * array of objects that define a "step" in the recipe. A step is simply an
 * object with a field "function" that specifies what is being done in the step
 * along with other fields that are semantically defined for that step.
 *
 * There are two types of recipes "builder" recipes and "combiner" recipes. Builder
 * recipes mutate an object until it matches some set of critera. Combiner
 * recipes take two objects, (a "left" and a "right"), and specify the steps
 * to merge the right object into the left object.
 *
 * A short nonsense example recipe is:
 * [ {"function": "get_url_domain", "path_length": 1, "field": "url", "dest": "url_domain"},
 *   {"function": "nb_tag", "fields": ["title", "description"]},
 *   {"function": "conditionally_nmf_tag", "fields": ["title", "description"]} ]
 *
 * Recipes are sandboxed by the fact that the step functions must be explicitly
 * whitelisted. Functions whitelisted for builder recipes are specifed in the
 * RecipeExecutor.ITEM_BUILDER_REGISTRY, while combiner functions are whitelisted
 * in RecipeExecutor.ITEM_COMBINER_REGISTRY .
 */
const RecipeExecutor = class RecipeExecutor {
  constructor(nbTaggers, nmfTaggers, tokenize) {
    this.ITEM_BUILDER_REGISTRY = {
      nb_tag: this.naiveBayesTag,
      conditionally_nmf_tag: this.conditionallyNmfTag,
      accept_item_by_field_value: this.acceptItemByFieldValue,
      tokenize_url: this.tokenizeUrl,
      get_url_domain: this.getUrlDomain,
      tokenize_field: this.tokenizeField,
      copy_value: this.copyValue,
      keep_top_k: this.keepTopK,
      scalar_multiply: this.scalarMultiply,
      elementwise_multiply: this.elementwiseMultiply,
      vector_multiply: this.vectorMultiply,
      scalar_add: this.scalarAdd,
      vector_add: this.vectorAdd,
      make_boolean: this.makeBoolean,
      whitelist_fields: this.whitelistFields,
      filter_by_value: this.filterByValue,
      l2_normalize: this.l2Normalize,
      prob_normalize: this.probNormalize,
      set_default: this.setDefault,
      lookup_value: this.lookupValue,
      copy_to_map: this.copyToMap,
      scalar_multiply_tag: this.scalarMultiplyTag,
      apply_softmax_tags: this.applySoftmaxTags,
    };
    this.ITEM_COMBINER_REGISTRY = {
      combiner_add: this.combinerAdd,
      combiner_max: this.combinerMax,
      combiner_collect_values: this.combinerCollectValues,
    };
    this.nbTaggers = nbTaggers;
    this.nmfTaggers = nmfTaggers;
    this.tokenize = tokenize;
  }

  /**
   * Determines the type of a field. Valid types are:
   *   string
   *   number
   *   array
   *   map (strings to anything)
   */
  _typeOf(data) {
    let t = typeof data;
    if (t === "object") {
      if (data === null) {
        return "null";
      }
      if (Array.isArray(data)) {
        return "array";
      }
      return "map";
    }
    return t;
  }

  /**
   * Returns a scalar, either because it was a constant, or by
   * looking it up from the item. Allows for a default value if the lookup
   * fails.
   */
  _lookupScalar(item, k, dfault) {
    if (this._typeOf(k) === "number") {
      return k;
    } else if (
      this._typeOf(k) === "string" &&
      k in item &&
      this._typeOf(item[k]) === "number"
    ) {
      return item[k];
    }
    return dfault;
  }

  /**
   * Simply appends all the strings from a set fields together. If the field
   * is a list, then the cells of the list are append.
   */
  _assembleText(item, fields) {
    let textArr = [];
    for (let field of fields) {
      if (field in item) {
        let type = this._typeOf(item[field]);
        if (type === "string") {
          textArr.push(item[field]);
        } else if (type === "array") {
          for (let ele of item[field]) {
            textArr.push(String(ele));
          }
        } else {
          textArr.push(String(item[field]));
        }
      }
    }
    return textArr.join(" ");
  }

  /**
   * Runs the naive bayes text taggers over a set of text fields. Stores the
   * results in new fields:
   *  nb_tags:         a map of text strings to probabilites
   *  nb_tokens:       the tokenized text that was tagged
   *
   * Config:
   *  fields:          an array containing a list of fields to concatenate and tag
   */
  naiveBayesTag(item, config) {
    let text = this._assembleText(item, config.fields);
    let tokens = this.tokenize(text);
    let tags = {};
    let extended_tags = {};

    for (let nbTagger of this.nbTaggers) {
      let result = nbTagger.tagTokens(tokens);
      if (result.label !== null && result.confident) {
        extended_tags[result.label] = result;
        tags[result.label] = Math.exp(result.logProb);
      }
    }
    item.nb_tags = tags;
    item.nb_tags_extended = extended_tags;
    item.nb_tokens = tokens;
    return item;
  }

  /**
   * Selectively runs NMF text taggers depending on which tags were found
   * by the naive bayes taggers. Writes the results in into new fields:
   *  nmf_tags_parent_weights:  map of pareent tags to probabilites of those parent tags
   *  nmf_tags:                 map of strings to maps of strings to probabilities
   *  nmf_tags_parent           map of child tags to parent tags
   *
   * Config:
   *  Not configurable
   */
  conditionallyNmfTag(item, config) {
    let nestedNmfTags = {};
    let parentTags = {};
    let parentWeights = {};

    if (!("nb_tags" in item) || !("nb_tokens" in item)) {
      return null;
    }

    Object.keys(item.nb_tags).forEach(parentTag => {
      let nmfTagger = this.nmfTaggers[parentTag];
      if (nmfTagger !== undefined) {
        nestedNmfTags[parentTag] = {};
        parentWeights[parentTag] = item.nb_tags[parentTag];
        let nmfTags = nmfTagger.tagTokens(item.nb_tokens);
        Object.keys(nmfTags).forEach(nmfTag => {
          nestedNmfTags[parentTag][nmfTag] = nmfTags[nmfTag];
          parentTags[nmfTag] = parentTag;
        });
      }
    });

    item.nmf_tags = nestedNmfTags;
    item.nmf_tags_parent = parentTags;
    item.nmf_tags_parent_weights = parentWeights;

    return item;
  }

  /**
   * Checks a field's value against another value (either from another field
   * or a constant). If the test passes, then the item is emitted, otherwise
   * the pipeline is aborted.
   *
   * Config:
   *  field      Field to read the value to test. Left side of operator.
   *  op         one of ==, !=, <, <=, >, >=
   *  rhsValue   Constant value to compare against. Right side of operator.
   *  rhsField   Field to read value to compare against. Right side of operator.
   *
   * NOTE: rhsValue takes precidence over rhsField.
   */
  acceptItemByFieldValue(item, config) {
    if (!(config.field in item)) {
      return null;
    }
    let rhs = null;
    if ("rhsValue" in config) {
      rhs = config.rhsValue;
    } else if ("rhsField" in config && config.rhsField in item) {
      rhs = item[config.rhsField];
    }
    if (rhs === null) {
      return null;
    }

    if (
      // eslint-disable-next-line eqeqeq
      (config.op === "==" && item[config.field] == rhs) ||
      // eslint-disable-next-line eqeqeq
      (config.op === "!=" && item[config.field] != rhs) ||
      (config.op === "<" && item[config.field] < rhs) ||
      (config.op === "<=" && item[config.field] <= rhs) ||
      (config.op === ">" && item[config.field] > rhs) ||
      (config.op === ">=" && item[config.field] >= rhs)
    ) {
      return item;
    }

    return null;
  }

  /**
   * Splits a URL into text-like tokens.
   *
   * Config:
   *  field   Field containing a URL
   *  dest    Field to write the tokens to as an array of strings
   *
   * NOTE: Any initial 'www' on the hostname is removed.
   */
  tokenizeUrl(item, config) {
    if (!(config.field in item)) {
      return null;
    }

    let url = new URL(item[config.field]);
    let domain = url.hostname;
    if (domain.startsWith("www.")) {
      domain = domain.substring(4);
    }
    let toks = this.tokenize(domain);
    let pathToks = this.tokenize(
      decodeURIComponent(url.pathname.replace(/\+/g, " "))
    );
    for (let tok of pathToks) {
      toks.push(tok);
    }
    for (let pair of url.searchParams.entries()) {
      let k = this.tokenize(decodeURIComponent(pair[0].replace(/\+/g, " ")));
      for (let tok of k) {
        toks.push(tok);
      }
      if (pair[1] !== null && pair[1] !== "") {
        let v = this.tokenize(decodeURIComponent(pair[1].replace(/\+/g, " ")));
        for (let tok of v) {
          toks.push(tok);
        }
      }
    }
    item[config.dest] = toks;

    return item;
  }

  /**
   * Gets the hostname (minus any initial "www." along with the left most
   * directories on the path.
   *
   * Config:
   *  field          Field containing the URL
   *  dest           Field to write the array of strings to
   *  path_length    OPTIONAL (DEFAULT: 0) Number of leftmost subdirectories to include
   */
  getUrlDomain(item, config) {
    if (!(config.field in item)) {
      return null;
    }

    let url = new URL(item[config.field]);
    let domain = url.hostname.toLocaleLowerCase();
    if (domain.startsWith("www.")) {
      domain = domain.substring(4);
    }
    item[config.dest] = domain;
    let pathLength = 0;
    if ("path_length" in config) {
      pathLength = config.path_length;
    }
    if (pathLength > 0) {
      item[config.dest] += url.pathname
        .toLocaleLowerCase()
        .split("/")
        .slice(0, pathLength + 1)
        .join("/");
    }

    return item;
  }

  /**
   * Splits a field into tokens.
   * Config:
   *  field         Field containing a string to tokenize
   *  dest          Field to write the array of strings to
   */
  tokenizeField(item, config) {
    if (!(config.field in item)) {
      return null;
    }

    item[config.dest] = this.tokenize(item[config.field]);

    return item;
  }

  /**
   * Deep copy from one field to another.
   * Config:
   *  src           Field to read from
   *  dest          Field to write to
   */
  copyValue(item, config) {
    if (!(config.src in item)) {
      return null;
    }

    item[config.dest] = JSON.parse(JSON.stringify(item[config.src]));

    return item;
  }

  /**
   * Converts a field containing a map of strings to a map of strings
   * to numbers, to a map of strings to numbers containing at most k elements.
   * This operation is performed by first, promoting all the subkeys up one
   * level, and then taking the top (or bottom) k values.
   *
   * Config:
   *  field         Points to a map of strings to a map of strings to numbers
   *  k             Maximum number of items to keep
   *  descending    OPTIONAL (DEFAULT: True) Sorts score in descending  order
   *                  (i.e. keeps maximum)
   */
  keepTopK(item, config) {
    if (!(config.field in item)) {
      return null;
    }
    let k = this._lookupScalar(item, config.k, 1048576);
    let descending = !("descending" in config) || config.descending !== false;

    // we can't sort by the values in the map, so we have to convert this
    // to an array, and then sort.
    let sortable = [];
    Object.keys(item[config.field]).forEach(outerKey => {
      let innerType = this._typeOf(item[config.field][outerKey]);
      if (innerType === "map") {
        Object.keys(item[config.field][outerKey]).forEach(innerKey => {
          sortable.push({
            key: innerKey,
            value: item[config.field][outerKey][innerKey],
          });
        });
      } else {
        sortable.push({ key: outerKey, value: item[config.field][outerKey] });
      }
    });

    sortable.sort((a, b) => {
      if (descending) {
        return b.value - a.value;
      }
      return a.value - b.value;
    });

    // now take the top k
    let newMap = {};
    let i = 0;
    for (let pair of sortable) {
      if (i >= k) {
        break;
      }
      newMap[pair.key] = pair.value;
      i++;
    }
    item[config.field] = newMap;

    return item;
  }

  /**
   * Scalar multiplies a vector by some constant
   *
   * Config:
   *  field         Points to:
   *                   a map of strings to numbers
   *                   an array of numbers
   *                   a number
   *  k             Either a number, or a string. If it's a number then This
   *                  is the scalar value to multiply by. If it's a string,
   *                  the value in the pointed to field is used.
   *  default       OPTIONAL (DEFAULT: 0), If k is a string, and no numeric
   *                  value is found, then use this value.
   */
  scalarMultiply(item, config) {
    if (!(config.field in item)) {
      return null;
    }
    let k = this._lookupScalar(item, config.k, config.dfault);

    let fieldType = this._typeOf(item[config.field]);
    if (fieldType === "number") {
      item[config.field] *= k;
    } else if (fieldType === "array") {
      for (let i = 0; i < item[config.field].length; i++) {
        item[config.field][i] *= k;
      }
    } else if (fieldType === "map") {
      Object.keys(item[config.field]).forEach(key => {
        item[config.field][key] *= k;
      });
    } else {
      return null;
    }

    return item;
  }

  /**
   * Elementwise multiplies either two maps or two arrays together, storing
   * the result in left. If left and right are of the same type, results in an
   * error.
   *
   * Maps are special case. For maps the left must be a nested map such as:
   * { k1: { k11: 1, k12: 2}, k2: { k21: 3, k22: 4 } } and right needs to be
   * simple map such as: { k1: 5, k2: 6} .  The operation is then to mulitply
   * every value of every right key, to every value every subkey where the
   * parent keys match. Using the previous examples, the result would be:
   * { k1: { k11: 5, k12: 10 }, k2: { k21: 18, k22: 24 } } .
   *
   * Config:
   *  left
   *  right
   */
  elementwiseMultiply(item, config) {
    if (!(config.left in item) || !(config.right in item)) {
      return null;
    }
    let leftType = this._typeOf(item[config.left]);
    if (leftType !== this._typeOf(item[config.right])) {
      return null;
    }
    if (leftType === "array") {
      if (item[config.left].length !== item[config.right].length) {
        return null;
      }
      for (let i = 0; i < item[config.left].length; i++) {
        item[config.left][i] *= item[config.right][i];
      }
    } else if (leftType === "map") {
      Object.keys(item[config.left]).forEach(outerKey => {
        let r = 0.0;
        if (outerKey in item[config.right]) {
          r = item[config.right][outerKey];
        }
        Object.keys(item[config.left][outerKey]).forEach(innerKey => {
          item[config.left][outerKey][innerKey] *= r;
        });
      });
    } else if (leftType === "number") {
      item[config.left] *= item[config.right];
    } else {
      return null;
    }

    return item;
  }

  /**
   * Vector multiplies (i.e. dot products) two vectors and stores the result in
   * third field. Both vectors must either by maps, or arrays of numbers with
   * the same length.
   *
   * Config:
   *   left       A field pointing to either a map of strings to numbers,
   *                or an array of numbers
   *   right      A field pointing to either a map of strings to numbers,
   *                or an array of numbers
   *   dest       The field to store the dot product.
   */
  vectorMultiply(item, config) {
    if (!(config.left in item) || !(config.right in item)) {
      return null;
    }

    let leftType = this._typeOf(item[config.left]);
    if (leftType !== this._typeOf(item[config.right])) {
      return null;
    }

    let destVal = 0.0;
    if (leftType === "array") {
      if (item[config.left].length !== item[config.right].length) {
        return null;
      }
      for (let i = 0; i < item[config.left].length; i++) {
        destVal += item[config.left][i] * item[config.right][i];
      }
    } else if (leftType === "map") {
      Object.keys(item[config.left]).forEach(key => {
        if (key in item[config.right]) {
          destVal += item[config.left][key] * item[config.right][key];
        }
      });
    } else {
      return null;
    }

    item[config.dest] = destVal;
    return item;
  }

  /**
   * Adds a constant value to all elements in the field. Mathematically,
   * this is the same as taking a 1-vector, scalar multiplying it by k,
   * and then vector adding it to a field.
   *
   * Config:
   *  field     A field pointing to either a map of strings to numbers,
   *                  or an array of numbers
   *  k             Either a number, or a string. If it's a number then This
   *                  is the scalar value to multiply by. If it's a string,
   *                  the value in the pointed to field is used.
   *  default       OPTIONAL (DEFAULT: 0), If k is a string, and no numeric
   *                  value is found, then use this value.
   */
  scalarAdd(item, config) {
    let k = this._lookupScalar(item, config.k, config.dfault);
    if (!(config.field in item)) {
      return null;
    }

    let fieldType = this._typeOf(item[config.field]);
    if (fieldType === "array") {
      for (let i = 0; i < item[config.field].length; i++) {
        item[config.field][i] += k;
      }
    } else if (fieldType === "map") {
      Object.keys(item[config.field]).forEach(key => {
        item[config.field][key] += k;
      });
    } else if (fieldType === "number") {
      item[config.field] += k;
    } else {
      return null;
    }

    return item;
  }

  /**
   * Adds two vectors together and stores the result in left.
   *
   * Config:
   *  left      A field pointing to either a map of strings to numbers,
   *                  or an array of numbers
   *  right     A field pointing to either a map of strings to numbers,
   *                  or an array of numbers
   */
  vectorAdd(item, config) {
    if (!(config.left in item)) {
      return this.copyValue(item, { src: config.right, dest: config.left });
    }
    if (!(config.right in item)) {
      return null;
    }

    let leftType = this._typeOf(item[config.left]);
    if (leftType !== this._typeOf(item[config.right])) {
      return null;
    }
    if (leftType === "array") {
      if (item[config.left].length !== item[config.right].length) {
        return null;
      }
      for (let i = 0; i < item[config.left].length; i++) {
        item[config.left][i] += item[config.right][i];
      }
      return item;
    } else if (leftType === "map") {
      Object.keys(item[config.right]).forEach(key => {
        let v = 0;
        if (key in item[config.left]) {
          v = item[config.left][key];
        }
        item[config.left][key] = v + item[config.right][key];
      });
      return item;
    }

    return null;
  }

  /**
   * Converts a vector from real values to boolean integers. (i.e. either 1/0
   * or 1/-1).
   *
   * Config:
   *   field            Field containing either a mpa of strings to numbers or
   *                      an array of numbers to  convert.
   *   threshold        OPTIONAL (DEFAULT: 0) Values above this will be replaced
   *                      with 1.0. Those below will be converted to 0.
   *   keep_negative    OPTIONAL (DEFAULT: False) If true, values below the
   *                      threshold will be converted to -1 instead of 0.
   */
  makeBoolean(item, config) {
    if (!(config.field in item)) {
      return null;
    }
    let threshold = this._lookupScalar(item, config.threshold, 0.0);
    let type = this._typeOf(item[config.field]);
    if (type === "array") {
      for (let i = 0; i < item[config.field].length; i++) {
        if (item[config.field][i] > threshold) {
          item[config.field][i] = 1.0;
        } else if (config.keep_negative) {
          item[config.field][i] = -1.0;
        } else {
          item[config.field][i] = 0.0;
        }
      }
    } else if (type === "map") {
      Object.keys(item[config.field]).forEach(key => {
        let value = item[config.field][key];
        if (value > threshold) {
          item[config.field][key] = 1.0;
        } else if (config.keep_negative) {
          item[config.field][key] = -1.0;
        } else {
          item[config.field][key] = 0.0;
        }
      });
    } else if (type === "number") {
      let value = item[config.field];
      if (value > threshold) {
        item[config.field] = 1.0;
      } else if (config.keep_negative) {
        item[config.field] = -1.0;
      } else {
        item[config.field] = 0.0;
      }
    } else {
      return null;
    }

    return item;
  }

  /**
   * Removes all keys from the item except for the ones specified.
   *
   * fields           An array of strings indicating the fields to keep
   */
  whitelistFields(item, config) {
    let newItem = {};
    for (let ele of config.fields) {
      if (ele in item) {
        newItem[ele] = item[ele];
      }
    }
    return newItem;
  }

  /**
   * Removes all keys whose value does not exceed some threshold.
   *
   * Config:
   *   field         Points to a map of strings to numbers
   *   threshold     Values must exceed this value, otherwise they are removed.
   */
  filterByValue(item, config) {
    if (!(config.field in item)) {
      return null;
    }
    let threshold = this._lookupScalar(item, config.threshold, 0.0);
    let filtered = {};
    Object.keys(item[config.field]).forEach(key => {
      let value = item[config.field][key];
      if (value > threshold) {
        filtered[key] = value;
      }
    });
    item[config.field] = filtered;

    return item;
  }

  /**
   * Rewrites a field so that its values are now L2 normed.
   *
   * Config:
   *  field         Points to a map of strings to numbers, or an array of numbers
   */
  l2Normalize(item, config) {
    if (!(config.field in item)) {
      return null;
    }
    let data = item[config.field];
    let type = this._typeOf(data);
    if (type === "array") {
      let norm = 0.0;
      for (let datum of data) {
        norm += datum * datum;
      }
      norm = Math.sqrt(norm);
      if (norm !== 0) {
        for (let i = 0; i < data.length; i++) {
          data[i] /= norm;
        }
      }
    } else if (type === "map") {
      let norm = 0.0;
      Object.keys(data).forEach(key => {
        norm += data[key] * data[key];
      });
      norm = Math.sqrt(norm);
      if (norm !== 0) {
        Object.keys(data).forEach(key => {
          data[key] /= norm;
        });
      }
    } else {
      return null;
    }

    item[config.field] = data;

    return item;
  }

  /**
   * Rewrites a field so that all of its values sum to 1.0
   *
   * Config:
   *  field         Points to a map of strings to numbers, or an array of numbers
   */
  probNormalize(item, config) {
    if (!(config.field in item)) {
      return null;
    }
    let data = item[config.field];
    let type = this._typeOf(data);
    if (type === "array") {
      let norm = 0.0;
      for (let datum of data) {
        norm += datum;
      }
      if (norm !== 0) {
        for (let i = 0; i < data.length; i++) {
          data[i] /= norm;
        }
      }
    } else if (type === "map") {
      let norm = 0.0;
      Object.keys(item[config.field]).forEach(key => {
        norm += item[config.field][key];
      });
      if (norm !== 0) {
        Object.keys(item[config.field]).forEach(key => {
          item[config.field][key] /= norm;
        });
      }
    } else {
      return null;
    }

    return item;
  }

  /**
   * Stores a value, if it is not already present
   *
   * Config:
   *  field             field to write to if it is missing
   *  value             value to store in that field
   */
  setDefault(item, config) {
    let val = this._lookupScalar(item, config.value, config.value);
    if (!(config.field in item)) {
      item[config.field] = val;
    }

    return item;
  }

  /**
   * Selctively promotes an value from an inner map up to the outer map
   *
   * Config:
   *  haystack            Points to a map of strings to values
   *  needle              Key inside the map we should promote up
   *  dest                Where we should write the value of haystack[needle]
   */
  lookupValue(item, config) {
    if (config.haystack in item && config.needle in item[config.haystack]) {
      item[config.dest] = item[config.haystack][config.needle];
    }

    return item;
  }

  /**
   * Demotes a field into a map
   *
   * Config:
   *  src               Field to copy
   *  dest_map          Points to a map
   *  dest_key          Key inside dest_map to copy src to
   */
  copyToMap(item, config) {
    if (config.src in item) {
      if (!(config.dest_map in item)) {
        item[config.dest_map] = {};
      }
      item[config.dest_map][config.dest_key] = item[config.src];
    }

    return item;
  }

  /**
   * Config:
   *  field             Points to a string to number map
   *  k                 Scalar to multiply the values by
   *  log_scale         Boolean, if true, then the values will be transformed
   *                      by a logrithm prior to multiplications
   */
  scalarMultiplyTag(item, config) {
    let EPSILON = 0.000001;
    if (!(config.field in item)) {
      return null;
    }
    let k = this._lookupScalar(item, config.k, 1);
    let type = this._typeOf(item[config.field]);
    if (type === "map") {
      Object.keys(item[config.field]).forEach(parentKey => {
        Object.keys(item[config.field][parentKey]).forEach(key => {
          let v = item[config.field][parentKey][key];
          if (config.log_scale) {
            v = Math.log(v + EPSILON);
          }
          item[config.field][parentKey][key] = v * k;
        });
      });
    } else {
      return null;
    }

    return item;
  }

  /**
   * Independently applies softmax across all subtags.
   *
   * Config:
   *   field        Points to a map of strings with values being another map of strings
   */
  applySoftmaxTags(item, config) {
    let type = this._typeOf(item[config.field]);
    if (type !== "map") {
      return null;
    }

    let abort = false;
    let softmaxSum = {};
    Object.keys(item[config.field]).forEach(tag => {
      if (this._typeOf(item[config.field][tag]) !== "map") {
        abort = true;
        return;
      }
      if (abort) {
        return;
      }
      softmaxSum[tag] = 0;
      Object.keys(item[config.field][tag]).forEach(subtag => {
        if (this._typeOf(item[config.field][tag][subtag]) !== "number") {
          abort = true;
          return;
        }
        let score = item[config.field][tag][subtag];
        softmaxSum[tag] += Math.exp(score);
      });
    });
    if (abort) {
      return null;
    }

    Object.keys(item[config.field]).forEach(tag => {
      Object.keys(item[config.field][tag]).forEach(subtag => {
        item[config.field][tag][subtag] =
          Math.exp(item[config.field][tag][subtag]) / softmaxSum[tag];
      });
    });

    return item;
  }

  /**
   * Vector adds a field and stores the result in left.
   *
   * Config:
   *   field              The field to vector add
   */
  combinerAdd(left, right, config) {
    if (!(config.field in right)) {
      return left;
    }
    let type = this._typeOf(right[config.field]);
    if (!(config.field in left)) {
      if (type === "map") {
        left[config.field] = {};
      } else if (type === "array") {
        left[config.field] = [];
      } else if (type === "number") {
        left[config.field] = 0;
      } else {
        return null;
      }
    }
    if (type !== this._typeOf(left[config.field])) {
      return null;
    }
    if (type === "map") {
      Object.keys(right[config.field]).forEach(key => {
        if (!(key in left[config.field])) {
          left[config.field][key] = 0;
        }
        left[config.field][key] += right[config.field][key];
      });
    } else if (type === "array") {
      for (let i = 0; i < right[config.field].length; i++) {
        if (i < left[config.field].length) {
          left[config.field][i] += right[config.field][i];
        } else {
          left[config.field].push(right[config.field][i]);
        }
      }
    } else if (type === "number") {
      left[config.field] += right[config.field];
    } else {
      return null;
    }

    return left;
  }

  /**
   * Stores the maximum value of the field in left.
   *
   * Config:
   *   field              The field to vector add
   */
  combinerMax(left, right, config) {
    if (!(config.field in right)) {
      return left;
    }
    let type = this._typeOf(right[config.field]);
    if (!(config.field in left)) {
      if (type === "map") {
        left[config.field] = {};
      } else if (type === "array") {
        left[config.field] = [];
      } else if (type === "number") {
        left[config.field] = 0;
      } else {
        return null;
      }
    }
    if (type !== this._typeOf(left[config.field])) {
      return null;
    }
    if (type === "map") {
      Object.keys(right[config.field]).forEach(key => {
        if (
          !(key in left[config.field]) ||
          right[config.field][key] > left[config.field][key]
        ) {
          left[config.field][key] = right[config.field][key];
        }
      });
    } else if (type === "array") {
      for (let i = 0; i < right[config.field].length; i++) {
        if (i < left[config.field].length) {
          if (left[config.field][i] < right[config.field][i]) {
            left[config.field][i] = right[config.field][i];
          }
        } else {
          left[config.field].push(right[config.field][i]);
        }
      }
    } else if (type === "number") {
      if (left[config.field] < right[config.field]) {
        left[config.field] = right[config.field];
      }
    } else {
      return null;
    }

    return left;
  }

  /**
   * Associates a value in right with another value in right. This association
   * is then stored in a map in left.
   *
   *     For example: If a sequence of rights is:
   *     { 'tags': {}, 'url_domain': 'maseratiusa.com/maserati', 'time': 41 }
   *     { 'tags': {}, 'url_domain': 'mbusa.com/mercedes',       'time': 21 }
   *     { 'tags': {}, 'url_domain': 'maseratiusa.com/maserati', 'time': 34 }
   *
   *     Then assuming a 'sum' operation, left can build a map that would look like:
   *     {
   *         'maseratiusa.com/maserati': 75,
   *         'mbusa.com/mercedes': 21,
   *     }
   *
   * Fields:
   *  left_field              field in the left to store / update the map
   *  right_key_field         Field in the right to use as a key
   *  right_value_field       Field in the right to use as a value
   *  operation               One of "sum", "max", "overwrite", "count"
   */
  combinerCollectValues(left, right, config) {
    let op;
    if (config.operation === "sum") {
      op = (a, b) => a + b;
    } else if (config.operation === "max") {
      op = (a, b) => (a > b ? a : b);
    } else if (config.operation === "overwrite") {
      op = (a, b) => b;
    } else if (config.operation === "count") {
      op = (a, b) => a + 1;
    } else {
      return null;
    }
    if (!(config.left_field in left)) {
      left[config.left_field] = {};
    }
    if (
      !(config.right_key_field in right) ||
      !(config.right_value_field in right)
    ) {
      return left;
    }

    let key = right[config.right_key_field];
    let rightValue = right[config.right_value_field];
    let leftValue = 0.0;
    if (key in left[config.left_field]) {
      leftValue = left[config.left_field][key];
    }

    left[config.left_field][key] = op(leftValue, rightValue);

    return left;
  }

  /**
   * Executes a recipe. Returns an object on success, or null on failure.
   */
  executeRecipe(item, recipe) {
    let newItem = item;
    if (recipe) {
      for (let step of recipe) {
        let op = this.ITEM_BUILDER_REGISTRY[step.function];
        if (op === undefined) {
          return null;
        }
        newItem = op.call(this, newItem, step);
        if (newItem === null) {
          break;
        }
      }
    }
    return newItem;
  }

  /**
   * Executes a recipe. Returns an object on success, or null on failure.
   */
  executeCombinerRecipe(item1, item2, recipe) {
    let newItem1 = item1;
    for (let step of recipe) {
      let op = this.ITEM_COMBINER_REGISTRY[step.function];
      if (op === undefined) {
        return null;
      }
      newItem1 = op.call(this, newItem1, item2, step);
      if (newItem1 === null) {
        break;
      }
    }

    return newItem1;
  }
};
