/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-env node */

const { loadCsf } = require("@storybook/csf-tools");
const { compile } = require("@storybook/mdx2-csf");
const { getStoryTitle, getMDXSource } = require("./markdown-story-utils.js");
const fs = require("fs");

/**
 * Function that tells Storybook how to index markdown based stories. This is
 * responsible for Storybook knowing how to populate the sidebar in the
 * Storybook UI, then retrieve the relevant file when a story is selected. In
 * order to get the data Storybook needs, we have to convert the markdown to
 * MDX, the convert that to CSF.
 * More info on indexers can be found here: storybook.js.org/docs/api/main-config-indexers
 * @param {string} fileName - Path to the file being processed.
 * @param {Object} opts - Options to configure the indexer.
 * @returns Array of IndexInput objects.
 */
module.exports = async (fileName, opts) => {
  // eslint-disable-next-line no-unsanitized/method
  const content = fs.readFileSync(fileName, "utf8");
  const title = getStoryTitle(fileName);
  const code = getMDXSource(content, title);

  // Compile MDX into CSF
  const csfCode = await compile(code, opts);

  // Parse CSF component
  let csf = loadCsf(csfCode, { fileName, makeTitle: () => title }).parse();

  // Return an array of story indexes.
  // Cribbed from https://github.com/storybookjs/storybook/blob/4169cd5b4ec9111de69f64a5e06edab9a6d2b0b8/code/addons/docs/src/preset.ts#L189
  const { indexInputs, stories } = csf;
  return indexInputs.map((input, index) => {
    const docsOnly = stories[index].parameters?.docsOnly;
    const tags = input.tags ? input.tags : [];
    if (docsOnly) {
      tags.push("stories-mdx-docsOnly");
    }
    // the mdx-csf compiler automatically adds the 'stories-mdx' tag to meta,
    // here' we're just making sure it is always there
    if (!tags.includes("stories-mdx")) {
      tags.push("stories-mdx");
    }
    return { ...input, tags };
  });
};
