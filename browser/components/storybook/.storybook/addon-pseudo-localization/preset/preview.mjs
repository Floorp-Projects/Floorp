/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file provides global decorators for the Storybook addon. In theory we
 * could combine multiple decorators, but for now we only need one.
 */

import {
  withPseudoLocalization,
  withFluentStrings,
} from "../withPseudoLocalization.mjs";

export const decorators = [withPseudoLocalization, withFluentStrings];
export const globalTypes = {
  pseudoStrategy: {
    name: "Pseudo l10n strategy",
    description: "Provides text variants for testing different locales.",
    defaultValue: "default",
  },
  fluentStrings: {
    name: "Fluent string map for components",
    description: "Mapping of component to fluent strings.",
    defaultValue: {},
  },
};
