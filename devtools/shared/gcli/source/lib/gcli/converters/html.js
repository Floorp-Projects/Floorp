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

/**
 * 'html' means a string containing HTML markup. We use innerHTML to inject
 * this into a DOM which has security implications, so this module will not
 * be used in all implementations.
 */
exports.items = [
  {
    item: 'converter',
    from: 'html',
    to: 'dom',
    exec: function(html, conversionContext) {
      var div = util.createElement(conversionContext.document, 'div');
      div.innerHTML = html;
      return div;
    }
  },
  {
    item: 'converter',
    from: 'html',
    to: 'string',
    exec: function(html, conversionContext) {
      var div = util.createElement(conversionContext.document, 'div');
      div.innerHTML = html;
      return div.textContent;
    }
  }
];
