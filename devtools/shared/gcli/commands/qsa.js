/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const l10n = require("gcli/l10n");

exports.items = [
  {
    item: "command",
    runAt: "server",
    name: "qsa",
    description: l10n.lookup("qsaDesc"),
    params: [{
      name: "query",
      type: "nodelist",
      description: l10n.lookup("qsaQueryDesc")
    }],
    exec: function(args, context) {
      return args.query.length;
    }
  }
];
