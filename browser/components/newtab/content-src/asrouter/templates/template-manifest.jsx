import {EOYSnippet} from "./EOYSnippet/EOYSnippet";
import {FXASignupSnippet} from "./FXASignupSnippet/FXASignupSnippet";
import {NewsletterSnippet} from "./NewsletterSnippet/NewsletterSnippet";
import {SendToDeviceSnippet} from "./SendToDeviceSnippet/SendToDeviceSnippet";
import {SimpleBelowSearchSnippet} from "./SimpleBelowSearchSnippet/SimpleBelowSearchSnippet";
import {SimpleSnippet} from "./SimpleSnippet/SimpleSnippet";

// Key names matching schema name of templates
export const SnippetsTemplates = {
  simple_snippet: SimpleSnippet,
  newsletter_snippet: NewsletterSnippet,
  fxa_signup_snippet: FXASignupSnippet,
  send_to_device_snippet: SendToDeviceSnippet,
  eoy_snippet: EOYSnippet,
  simple_below_search_snippet: SimpleBelowSearchSnippet,
};
