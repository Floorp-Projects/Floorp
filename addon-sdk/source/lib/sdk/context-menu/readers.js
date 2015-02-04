/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const { Class } = require("../core/heritage");
const { extend } = require("../util/object");
const { memoize, method, identity } = require("../lang/functional");

const serializeCategory = ({type}) => ({ category: `reader/${type}()` });

const Reader = Class({
  initialize() {
    this.id = `reader/${this.type}()`
  },
  toJSON() {
    return serializeCategory(this);
  }
});


const MediaTypeReader = Class({ extends: Reader, type: "MediaType" });
exports.MediaType = MediaTypeReader;

const LinkURLReader = Class({ extends: Reader, type: "LinkURL" });
exports.LinkURL = LinkURLReader;

const SelectionReader = Class({ extends: Reader, type: "Selection" });
exports.Selection = SelectionReader;

const isPageReader = Class({ extends: Reader, type: "isPage" });
exports.isPage = isPageReader;

const isFrameReader = Class({ extends: Reader, type: "isFrame" });
exports.isFrame = isFrameReader;

const isEditable = Class({ extends: Reader, type: "isEditable"});
exports.isEditable = isEditable;



const ParameterizedReader = Class({
  extends: Reader,
  readParameter: function(value) {
    return value;
  },
  toJSON: function() {
    var json = serializeCategory(this);
    json[this.parameter] = this[this.parameter];
    return json;
  },
  initialize(...params) {
    if (params.length) {
      this[this.parameter] = this.readParameter(...params);
    }
    this.id = `reader/${this.type}(${JSON.stringify(this[this.parameter])})`;
  }
});
exports.ParameterizedReader = ParameterizedReader;


const QueryReader = Class({
  extends: ParameterizedReader,
  type: "Query",
  parameter: "path"
});
exports.Query = QueryReader;


const AttributeReader = Class({
  extends: ParameterizedReader,
  type: "Attribute",
  parameter: "name"
});
exports.Attribute = AttributeReader;

const SrcURLReader = Class({
  extends: AttributeReader,
  name: "src",
});
exports.SrcURL = SrcURLReader;

const PageURLReader = Class({
  extends: QueryReader,
  path: "ownerDocument.URL",
});
exports.PageURL = PageURLReader;

const SelectorMatchReader = Class({
  extends: ParameterizedReader,
  type: "SelectorMatch",
  parameter: "selector"
});
exports.SelectorMatch = SelectorMatchReader;

const extractors = new WeakMap();
extractors.id = 0;


var Extractor = Class({
  extends: ParameterizedReader,
  type: "Extractor",
  parameter: "source",
  initialize: function(f) {
    this[this.parameter] = String(f);
    if (!extractors.has(f)) {
      extractors.id = extractors.id + 1;
      extractors.set(f, extractors.id);
    }

    this.id = `reader/${this.type}.for(${extractors.get(f)})`
  }
});
exports.Extractor = Extractor;
