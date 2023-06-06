# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### These strings are used inside the Accessibility panel.

accessibility-learn-more = Learn more

accessibility-text-label-header = Text Labels and Names

accessibility-keyboard-header = Keyboard

## These strings are used in the overlay displayed when running an audit in the accessibility panel

accessibility-progress-initializing = Initializing…
  .aria-valuetext = Initializing…

# This string is displayed in the audit progress bar in the accessibility panel.
# Variables:
#   $nodeCount (Integer) - The number of nodes for which the audit was run so far.
accessibility-progress-progressbar =
    { $nodeCount ->
        [one] Checking { $nodeCount } node
       *[other] Checking { $nodeCount } nodes
    }

accessibility-progress-finishing = Finishing up…
  .aria-valuetext = Finishing up…

## Text entries that are used as text alternative for icons that depict accessibility issues.

accessibility-warning =
  .alt = Warning

accessibility-fail =
  .alt = Error

accessibility-best-practices =
  .alt = Best Practices

## Text entries for a paragraph used in the accessibility panel sidebar's checks section
## that describe that currently selected accessible object has an accessibility issue
## with its text label or accessible name.

accessibility-text-label-issue-area = Use <code>alt</code> attribute to label <div>area</div> elements that have the <span>href</span> attribute. <a>Learn more</a>

accessibility-text-label-issue-dialog = Dialogs should be labeled. <a>Learn more</a>

accessibility-text-label-issue-document-title = Documents must have a <code>title</code>. <a>Learn more</a>

accessibility-text-label-issue-embed = Embedded content must be labeled. <a>Learn more</a>

accessibility-text-label-issue-figure = Figures with optional captions should be labeled. <a>Learn more</a>

accessibility-text-label-issue-fieldset = <code>fieldset</code> elements must be labeled. <a>Learn more</a>

accessibility-text-label-issue-fieldset-legend2 = Use a <code>legend</code> element to label a <span>fieldset</span>. <a>Learn more</a>

accessibility-text-label-issue-form = Form elements must be labeled. <a>Learn more</a>

accessibility-text-label-issue-form-visible = Form elements should have a visible text label. <a>Learn more</a>

accessibility-text-label-issue-frame = <code>frame</code> elements must be labeled. <a>Learn more</a>

accessibility-text-label-issue-glyph = Use <code>alt</code> attribute to label <span>mglyph</span> elements. <a>Learn more</a>

accessibility-text-label-issue-heading = Headings must be labeled. <a>Learn more</a>

accessibility-text-label-issue-heading-content = Headings should have visible text content. <a>Learn more</a>

accessibility-text-label-issue-iframe = Use <code>title</code> attribute to describe <span>iframe</span> content. <a>Learn more</a>

accessibility-text-label-issue-image = Content with images must be labeled. <a>Learn more</a>

accessibility-text-label-issue-interactive = Interactive elements must be labeled. <a>Learn more</a>

accessibility-text-label-issue-optgroup-label2 = Use a <code>label</code> attribute to label an <span>optgroup</span>. <a>Learn more</a>

accessibility-text-label-issue-toolbar = Toolbars must be labeled when there is more than one toolbar. <a>Learn more</a>

## Text entries for a paragraph used in the accessibility panel sidebar's checks section
## that describe that currently selected accessible object has a keyboard accessibility
## issue.

accessibility-keyboard-issue-semantics = Focusable elements should have interactive semantics. <a>Learn more</a>

accessibility-keyboard-issue-tabindex = Avoid using <code>tabindex</code> attribute greater than zero. <a>Learn more</a>

accessibility-keyboard-issue-action = Interactive elements must be able to be activated using a keyboard. <a>Learn more</a>

accessibility-keyboard-issue-focusable = Interactive elements must be focusable. <a>Learn more</a>

accessibility-keyboard-issue-focus-visible = Focusable element may be missing focus styling. <a>Learn more</a>

accessibility-keyboard-issue-mouse-only = Clickable elements must be focusable and should have interactive semantics. <a>Learn more</a>
