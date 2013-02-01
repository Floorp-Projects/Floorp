/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Loader, resolveURI, Require,
        unload, override, descriptor  } = require('../loader/cuddlefish');

exports.Loader = function(module, globals, packaging) {
  let options = packaging || require("@loader/options");
  options = override(options, {
    globals: override(require('../system/globals'), globals || {})
  });

  let loader = Loader(options);
  return Object.create(loader, descriptor({
    require: Require(loader, module),
    sandbox: function(id) {
      let requirement = loader.resolve(id, module.id);
      let uri = resolveURI(requirement, loader.mapping);
      return loader.sandboxes[uri];
    },
    unload: function(reason) {
      unload(loader, reason);
    }
  }));
};
