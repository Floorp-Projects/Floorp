/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var path = require('path');
var i18n = require('i18n-abide');

module.exports = function (grunt) {
  'use strict';

  // convert localized TOS agreement from markdown to html partials.

  function rename(destPath, srcFile) {
    // Normalize the filenames to use the locale name.
    var lang = srcFile.replace('.md', '');
    return path.join(destPath, i18n.localeFrom(lang) + '.html');
  }

  grunt.config('marked', {
    options: {
      sanitize: false,
      gfm: true
    },
    tos: {
      files: [
        {
          expand: true,
          cwd: '<%= project_vars.tos_md_src %>',
          src: ['**/*.md'],
          dest: '<%= project_vars.tos_html_dest %>',
          rename: rename
        }
      ]
    }
  });
};
