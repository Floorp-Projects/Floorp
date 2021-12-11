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

The deadline for accessibility review requests is Friday of the first week of
nightly builds for the release in which the feature/change is expected to ship.
This is the same date as the PI Request deadline.

## How Do I Request Accessibility Review?
You request accessibility review by setting the a11y-review flag to "requested"
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
  or Slack
* Email: accessibility@mozilla.com
