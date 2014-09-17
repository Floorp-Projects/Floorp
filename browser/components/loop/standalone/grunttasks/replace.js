/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

module.exports = function (grunt) {
  'use strict';

  grunt.config('replace', {
    tos: {
      src: [
        '<%= project_vars.tos_md_src %>/*.md'
      ],
      overwrite: true,
      replacements: [{
        // remove tags not handle by the markdown-to-HTML conversation
        from: /{:\s.*?\s}/g,
        to: ''
      }, {
        // remove unhandled comments
        from: /^#\s.*?\n$/m,
        to: ''
      }, {
        // fix "smart quotes"
        from: /(“|”)/g,
        to: "&quot;"
      }, {
        // fix "smart quotes"
        from: /’/g,
        to: "&#39;"
      }, {
        from: /–/g,
        to: "&mdash;"
      }
      ]
    }
  });
};
