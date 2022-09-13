# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Variables:
#   $expiryDate (string) - date on which the colorway collection expires. When formatting this, you may omit the year, only exposing the month and day, as colorway collections will always expire within a year.
colorway-collection-expiry-label = Expires { DATETIME($expiryDate, month: "long", day: "numeric") }

# Document title, not shown in the UI but exposed through accessibility APIs
colorways-modal-title = Choose Your Colorway

colorway-intensity-selector-label = Intensity
colorway-intensity-soft = Soft
colorway-intensity-balanced = Balanced
# "Bold" is used in the sense of bravery or courage, not in the sense of
# emphasized text.
colorway-intensity-bold = Bold

# Label for the button to keep using the selected colorway in the browser
colorway-closet-set-colorway-button = Set colorway
colorway-closet-cancel-button = Cancel

colorway-homepage-reset-prompt = Make { -firefox-home-brand-name } your colorful homepage
colorway-homepage-reset-success-message = { -firefox-home-brand-name } is now your homepage
colorway-homepage-reset-apply-button = Apply
colorway-homepage-reset-undo-button = Undo
