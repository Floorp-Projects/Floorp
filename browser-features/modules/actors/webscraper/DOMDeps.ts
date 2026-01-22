/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { HighlightManager } from "./HighlightManager.ts";
import type { EventDispatcher } from "./EventDispatcher.ts";
import type { TranslationHelper } from "./TranslationHelper.ts";
import type { WebScraperContext } from "./types.ts";

export interface DOMOpsDeps {
  context: WebScraperContext;
  highlightManager: HighlightManager;
  eventDispatcher: EventDispatcher;
  translationHelper: TranslationHelper;
  getContentWindow: () => (Window & typeof globalThis) | null;
  getDocument: () => Document | null;
}
