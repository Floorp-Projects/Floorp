# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## Settings

site-data-settings-window =
    .title = Manage Cookies and Site Data

site-data-settings-description = The following websites store cookies and site data on your computer. { -brand-short-name } keeps data from websites with persistent storage until you delete it, and deletes data from websites with non-persistent storage as space is needed.

site-data-search-textbox =
    .placeholder = Search websites
    .accesskey = S

site-data-column-host =
    .label = Site
site-data-column-cookies =
    .label = Cookies
site-data-column-storage =
    .label = Storage
site-data-column-last-used =
    .label = Last Used

# This label is used in the "Host" column for local files, which have no host.
site-data-local-file-host = (local file)

site-data-remove-selected =
    .label = Remove Selected
    .accesskey = R

site-data-settings-dialog =
    .buttonlabelaccept = Save Changes
    .buttonaccesskeyaccept = a

# Variables:
#   $value (Number) - Value of the unit (for example: 4.6, 500)
#   $unit (String) - Name of the unit (for example: "bytes", "KB")
site-storage-usage =
    .value = { $value } { $unit }
site-storage-persistent =
    .value = { site-storage-usage.value } (Persistent)

site-data-remove-all =
    .label = Remove All
    .accesskey = e

site-data-remove-shown =
    .label = Remove All Shown
    .accesskey = e

## Removing

site-data-removing-dialog =
    .title = { site-data-removing-header }
    .buttonlabelaccept = Remove

site-data-removing-header = Removing Cookies and Site Data

site-data-removing-desc = Removing cookies and site data may log you out of websites. Are you sure you want to make the changes?
# Variables:
#   $baseDomain (String) - The single domain for which data is being removed
site-data-removing-single-desc = Removing cookies and site data may log you out of websites. Are you sure you want to remove cookies and site data for <strong>{ $baseDomain }</strong>?

site-data-removing-table = Cookies and site data for the following websites will be removed
