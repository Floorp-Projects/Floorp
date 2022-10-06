# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## Subframe crash notification

crashed-subframe-message = <strong>Part of this page crashed.</strong> To let { -brand-product-name } know about this issue and get it fixed faster, please submit a report.

# The string for crashed-subframe-title.title should match crashed-subframe-message,
# but without any markup.
crashed-subframe-title =
  .title = Part of this page crashed. To let { -brand-product-name } know about this issue and get it fixed faster, please submit a report.
crashed-subframe-learnmore-link =
  .value = Learn more
crashed-subframe-submit =
  .label = Submit report
  .accesskey = S

## Pending crash reports

# Variables:
#   $reportCount (Number): the number of pending crash reports
pending-crash-reports-message =
    { $reportCount ->
        [one] You have an unsent crash report
       *[other] You have { $reportCount } unsent crash reports
    }
pending-crash-reports-view-all =
    .label = View
pending-crash-reports-send =
    .label = Send
pending-crash-reports-always-send =
    .label = Always send
