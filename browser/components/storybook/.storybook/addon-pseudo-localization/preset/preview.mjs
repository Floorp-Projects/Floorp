/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file provides global decorators for the Storybook addon. In theory we
 * could combine multiple decorators, but for now we only need one.
 */

import { withPseudoLocalization } from "../withPseudoLocalization.mjs";

export const decorators = [withPseudoLocalization];
export const globalTypes = {
  pseudoStrategy: {
    name: "Pseudo l10n strategy",
    description: "Provides text variants for testing different locales.",
    defaultValue: "default",
  },
};
