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

var l10n = require('../util/l10n');
var cli = require('../cli');

exports.items = [
  {
    // A type that lists available languages
    item: 'type',
    name: 'language',
    parent: 'selection',
    lookup: function(context) {
      return context.system.languages.getAll().map(function(language) {
        return { name: language.name, value: language };
      });
    }
  },
  {
    // A command to switch languages
    item: 'command',
    name: 'lang',
    description: l10n.lookup('langDesc'),
    params: [
      {
        name: 'language',
        type: 'language'
      }
    ],
    returnType: 'view',
    exec: function(args, context) {
      var terminal = cli.getMapping(context).terminal;

      context.environment.window.setTimeout(function() {
        terminal.switchLanguage(args.language);
      }, 10);

      return {
        html:
          '<div class="gcli-section ${style}">' +
          '  ${langOutput}' +
          '</div>',
        data: {
          langOutput: l10n.lookupFormat('langOutput', [ args.language.name ]),
          style: args.language.proportionalFonts ? '' : 'gcli-row-script'
        }
      };
    }
  }
];
