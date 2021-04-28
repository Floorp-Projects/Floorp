/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

const { exec } = require("child_process");

const basePath = `./content/panels/tmpl/`;

let templates = [
  `ho2/ho2_articleinfo`,
  `ho2/ho2_download`,
  `ho2/ho2_download_error`,
  `ho2/ho2_sharebutton`,
  `loggedoutvariants/variant_a`,
  `loggedoutvariants/variant_b`,
  `loggedoutvariants/variant_c`,
  `saved_premiumextras`,
  `saved_premiumshell`,
  `saved_shell`,
  `saved_tmplogin`,
  `signup_shell`,
  `home_shell`,
  `popular_topics`,
  `explore_more`,
  `signupstoryboard_shell`,
  `item_recs`,
];

// Append extension and base path
templates = templates.map(templatePath => {
  return `${basePath}${templatePath}.handlebars`;
});

// "build:handlebars": "npx handlebars ./content/panels/tmpl/ho2/ho2_articleinfo.handlebars -f handlebarstest.js",
console.log(`Compiling Handlebars\n`);

templates.forEach(path => {
  console.log(`Building: ${path}`);
});

let complilationGroup = templates.reduce((accumulator, currentValue) => {
  return `${accumulator} ${currentValue}`;
});

exec(
  `npx handlebars ${complilationGroup} -f ./content/panels/js/tmpl.js`,
  (error, stdout, stderr) => {
    if (stderr) {
      console.error(stderr);
    }
  }
);
