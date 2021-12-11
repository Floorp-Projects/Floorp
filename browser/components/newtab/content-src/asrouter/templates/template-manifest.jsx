/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { EOYSnippet } from "./EOYSnippet/EOYSnippet";
import { FXASignupSnippet } from "./FXASignupSnippet/FXASignupSnippet";
import { NewsletterSnippet } from "./NewsletterSnippet/NewsletterSnippet";
import {
  SendToDeviceSnippet,
  SendToDeviceScene2Snippet,
} from "./SendToDeviceSnippet/SendToDeviceSnippet";
import { SimpleBelowSearchSnippet } from "./SimpleBelowSearchSnippet/SimpleBelowSearchSnippet";
import { SimpleSnippet } from "./SimpleSnippet/SimpleSnippet";

// Key names matching schema name of templates
export const SnippetsTemplates = {
  simple_snippet: SimpleSnippet,
  newsletter_snippet: NewsletterSnippet,
  fxa_signup_snippet: FXASignupSnippet,
  send_to_device_snippet: SendToDeviceSnippet,
  send_to_device_scene2_snippet: SendToDeviceScene2Snippet,
  eoy_snippet: EOYSnippet,
  simple_below_search_snippet: SimpleBelowSearchSnippet,
};
