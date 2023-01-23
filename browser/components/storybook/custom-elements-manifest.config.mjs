/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Custom element manifest analyzer plugin to remove specific properties from
 * custom-elements.json that we don't want to document in our Storybook props tables.
 */
function removeExcludedProperties() {
  const EXCLUDED_PROPERTIES = [
    "SUPPORT_URL",
    "LOCAL_NAME",
    "queries",
    "stylesheetUrl",
    "shadowRootOptions",
  ];
  return {
    packageLinkPhase({ customElementsManifest }) {
      customElementsManifest?.modules?.forEach(module => {
        module?.declarations?.forEach(declaration => {
          if (declaration.members != null) {
            declaration.members = declaration.members.filter(member => {
              return (
                !member.kind === "field" ||
                !EXCLUDED_PROPERTIES.includes(member.name)
              );
            });
          }
        });
      });
    },
  };
}

/**
 * Custom element manifest config. Controls how we parse directories for
 * custom elements to populate custom-elements.json.
 */
const config = {
  globs: ["../../../toolkit/content/widgets/**/*.mjs"],
  exclude: [
    "../../../toolkit/content/widgets/**/*.stories.mjs",
    "../../../toolkit/content/widgets/vendor/**",
    "../../../toolkit/content/widgets/lit-utils.mjs",
  ],
  outdir: ".",
  litelement: true,
  plugins: [removeExcludedProperties()],
};

export default config;
