/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");

const Manifest = createFactory(
  require("devtools/client/application/src/components/manifest/Manifest")
);

const { MANIFEST_DATA } = require("../../../src/constants");

// needs to move to reducer
const data = {
  warnings: MANIFEST_DATA.moz_validation,
  icons: MANIFEST_DATA.icons,
  identity: {
    name: MANIFEST_DATA.name,
    short_name: MANIFEST_DATA.short_name,
  },
  presentation: {
    display: MANIFEST_DATA.display,
    orientation: MANIFEST_DATA.orientation,
    start_url: MANIFEST_DATA.start_url,
    theme_color: MANIFEST_DATA.theme_color,
    background_color: MANIFEST_DATA.background_color,
  },
};

/**
 * Test for Manifest component
 */

describe("Manifest", () => {
  it("renders the expected snapshot", () => {
    const wrapper = shallow(
      Manifest({
        identity: data.identity,
        warnings: data.warnings,
        icons: data.icons,
        presentation: data.presentation,
      })
    );
    expect(wrapper).toMatchSnapshot();
  });
});
