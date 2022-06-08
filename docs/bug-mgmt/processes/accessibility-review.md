# Accessibility Review

## Introduction
At Mozilla, accessibility is a fundamental part of our mission to ensure the
internet is "open and accessible to all," helping to empower people, regardless
of their abilities, to contribute to the common good. Accessibility Review is a
service provided by the Mozilla Accessibility team to review features and
changes to ensure they are accessible to and inclusive of people with
disabilities.

## Do I Need Accessibility Review?
You should consider requesting accessibility review if you aren't certain
whether your change is accessible to people with disabilities. Accessibility
review is optional, but it is strongly encouraged if you are introducing new
user interface or are significantly redesigning existing user interface.
Review should be requested both on the design side _and_ on the engineering side.

## When Should I Request Accessibility Review?
Generally, it's best to request accessibility review as early as possible, even
during the product requirements or UI design stage. Particularly for more
complex user interfaces, accessibility is much easier when incorporated into the
design, rather than attempting to retro-fit accessibility after the
implementation is well underway.

The accessibility team has developed the [Mozilla Accessibility Release
Guidelines](https://wiki.mozilla.org/Accessibility/Guidelines) which outline
what is needed to make user interfaces accessible. To make accessibility review
faster, you may wish to try to verify and implement these guidelines prior to
requesting accessibility review.

For design reviews, please allow at least a week between review request and expected-engineering-handoff. The deadline for engineering review requests is Friday of the first week of nightly builds for the release in which the feature/change is expected to ship.
This is the same date as the PI Request deadline.

## Requesting Design Review
Design review should be requested via the #accessibility slack channel. Please post the following information to help us triage your review:

```
Timeline? (ie. when is engineering handoff? product approval? etc.)
Tracking/bug issue:
Product spec:
Figma file:
Engineering lead:
Product manager:
Have you completed self-review (contrast audit, focus order/role annotations, HCM mockups)?
```

In addition to posting this information, please complete the following self-review tasks **before requesting review**. Note: Some of the following links require SSO authentication.

- **Perform a contrast audit**: Using a [figma plugin that audits contrast](https://www.figma.com/community/plugin/748533339900865323/Contrast), check your designs for color contrast sufficiency. Your designs should be at least "AA" rated in order to pass accessibility review. "AAA" is even better! If there are particular components that are difficult to adjust to meet "AA" standards, make a note in the figma file and the a11y team will provide specific guidance during review.
- **Add focus order and role annotations**: Focus order annotations describe the behaviour a keyboard user should expect when navigating your design. Generally this follows the reading order. Note that we only care about focusable elements here (ie. links, buttons, inputs, etc.).
Role annotations help screen readers and other assistive technologies identify the "kind" of component they're navigating through. These mappings expose semantic information to the user. You can find a [list of common roles here](https://www.codeinwp.com/blog/wai-aria-roles/). You may want to use a [figma plugin that annotates focus and roles](https://www.figma.com/community/plugin/731310036968334777/A11y---Focus-Order) for this process. You do not need to annotate every view in your design, pick those with the largest amount of new content.
- **Create Windows High-Contrast Mode (HCM) mockups**: Our designs should be accessible to users [running HCM](https://docs.google.com/document/d/1El3XJiAdA5gFcG7H9iI1dNmLbht0hXfi_oKBZPWx3t0/edit). You can read more about [how HCM affects color selection](https://firefox-source-docs.mozilla.org/accessible/ColorsAndHighContrastMode.html), and [how to design for HCM](https://wiki.mozilla.org/Accessibility/Design_Guide). Using the ["Night Sky" HCM palette](https://www.figma.com/file/XQrEePCCJebjlVBQwNggQ6/Pro-Client-Accessibility-Reviews?node-id=25%3A3848), translate your designs into High Contrast. Remember, it's important we use these colors **semantically**, not based on a desire for a particular aesthetic. Colors are labelled according to their uses -- `Background` for page background, `Button Text` for button or control text, `Selected Item Background` for backgrounds of selected or active items, etc.. You do not need to mock up every view in your design, pick those with the largest amount of new content. You can find [examples of previous HCM mock ups here](https://www.figma.com/file/XQrEePCCJebjlVBQwNggQ6/Accessibility).
Where possible, we should rely on SVG's and PNG's for image content to increase adaptability.


## Requesting Engineering Review
For an engineering-focused review, you submit a review request by setting the a11y-review flag to "requested"
on a bug in Bugzilla and filling in the template that appears in the comment
field. For features spanning several bugs, you may wish to file a new, dedicated
bug for the accessibility review. Otherwise, particularly for smaller changes,
you may do this on an existing bug. Note that if you file a new bug, you will
need to submit the bug and then edit it to set the flag.

## Questions?
If you have any questions, please don't hesitate to contact the Accessibility
team:

* \#accessibility on
  [Matrix](https://matrix.to/#/!jmuErVonajdNMbgdeY:mozilla.org?via=mozilla.org&via=matrix.org)
  or [Slack](https://mozilla.slack.com/archives/C4E0W8B8E)
* Email: accessibility@mozilla.com
* Please avoid reaching out to individual team members directly -- containing review requests and questions in these channels helps us balance our workload. Thank you!
