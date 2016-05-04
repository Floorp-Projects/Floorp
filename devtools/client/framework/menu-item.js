/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * A partial implementation of the MenuItem API provided by electron:
 * https://github.com/electron/electron/blob/master/docs/api/menu-item.md.
 *
 * Missing features:
 *   - id String - Unique within a single menu. If defined then it can be used
 *                 as a reference to this item by the position attribute.
 *   - role String - Define the action of the menu item; when specified the
 *                   click property will be ignored
 *   - sublabel String
 *   - accelerator Accelerator
 *   - icon NativeImage
 *   - position String - This field allows fine-grained definition of the
 *                       specific location within a given menu.
 *
 * Implemented features:
 *  @param Object options
 *    Function click
 *      Will be called with click(menuItem, browserWindow) when the menu item
 *       is clicked
 *    String type
 *      Can be normal, separator, submenu, checkbox or radio
 *    String label
 *    Boolean enabled
 *      If false, the menu item will be greyed out and unclickable.
 *    Boolean checked
 *      Should only be specified for checkbox or radio type menu items.
 *    Menu submenu
 *      Should be specified for submenu type menu items. If submenu is specified,
 *      the type: 'submenu' can be omitted. If the value is not a Menu then it
 *      will be automatically converted to one using Menu.buildFromTemplate.
 *    Boolean visible
 *      If false, the menu item will be entirely hidden.
 */
function MenuItem({
    accesskey = null,
    checked = false,
    click = () => {},
    disabled = false,
    label = "",
    id = null,
    submenu = null,
    type = "normal",
    visible = true,
} = { }) {
  this.accesskey = accesskey;
  this.checked = checked;
  this.click = click;
  this.disabled = disabled;
  this.id = id;
  this.label = label;
  this.submenu = submenu;
  this.type = type;
  this.visible = visible;
}

module.exports = MenuItem;
