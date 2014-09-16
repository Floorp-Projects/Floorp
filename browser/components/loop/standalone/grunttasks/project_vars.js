/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

module.exports = function (grunt) {
  'use strict';

  var TEMPLATE_ROOT = 'content/legal';
  var TOS_PP_REPO_ROOT = 'bower_components/tos-pp';

  grunt.config('project_vars', {
    app: "content",
    // Translated TOS/PP agreements.
    tos_pp_repo_dest: TOS_PP_REPO_ROOT,
    tos_md_src: TOS_PP_REPO_ROOT + '/WebRTC_ToS/',
    tos_html_dest: TEMPLATE_ROOT + '/terms'
  });
};
