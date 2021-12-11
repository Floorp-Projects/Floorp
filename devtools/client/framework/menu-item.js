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
 *   - position String - This field allows fine-grained definition of the
 *                       specific location within a given menu.
 *
 * Implemented features:
 *  @param Object options
 *    String accelerator
 *      Text that appears beside the menu label to indicate the shortcut key
 *      (accelerator key) to use to invoke the command.
 *      Unlike the Electron API, this is a label only and does not actually
 *      register a handler for the key.
 *    String accesskey [non-standard]
 *      A single character used as the shortcut key. This should be one of the
 *      characters that appears in the label.
 *    Function click
 *      Will be called with click(menuItem, browserWindow) when the menu item
 *       is clicked
 *    String type
 *      Can be normal, separator, submenu, checkbox or radio
 *    String label
 *    String image
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
  accelerator = null,
  accesskey = null,
  l10nID = null,
  checked = false,
  click = () => {},
  disabled = false,
  hover = () => {},
  id = null,
  label = "",
  image = null,
  submenu = null,
  type = "normal",
  visible = true,
} = {}) {
  this.accelerator = accelerator;
  this.accesskey = accesskey;
  this.l10nID = l10nID;
  this.checked = checked;
  this.click = click;
  this.disabled = disabled;
  this.hover = hover;
  this.id = id;
  this.label = label;
  this.image = image;
  this.submenu = submenu;
  this.type = type;
  this.visible = visible;
}

module.exports = MenuItem;
