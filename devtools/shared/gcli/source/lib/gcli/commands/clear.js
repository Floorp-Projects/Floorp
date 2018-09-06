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

exports.items = [
  {
    // A command to clear the output area
    item: 'command',
    runAt: 'client',
    name: 'clear',
    description: l10n.lookup('clearDesc'),
    returnType: 'clearoutput',
    exec: function(args, context) { }
  },
  {
    item: 'converter',
    from: 'clearoutput',
    to: 'view',
    exec: function(ignore, conversionContext) {
      return {
        html: '<span onload="${onload}"></span>',
        data: {
          onload: function(ev) {
            // element starts off being the span above, and we walk up the
            // tree looking for the terminal
            var element = ev.target;
            while (element != null && element.terminal == null) {
              element = element.parentElement;
            }

            if (element == null) {
              // This is only an event handler on a completed command
              // So we're relying on this showing up in the console
              throw new Error('Failed to find clear');
            }

            element.terminal.clear();
          }
        }
      };
    }
  }
];
