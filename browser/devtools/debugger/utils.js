/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const utils = {
  /**
   * Create <command> elements within `commandset` with event handlers
   * bound to the `command` event
   *
   * @param commandset HTML Element
   *        A <commandset> element
   * @param commands Object
   *        An object where keys specify <command> ids and values
   *        specify event handlers to be bound on the `command` event
   */
  addCommands: function(commandset, commands) {
    Object.keys(commands).forEach(name => {
      let node = document.createElement('command');
      node.id = name;
      // XXX bug 371900: the command element must have an oncommand
      // attribute as a string set by `setAttribute` for keys to use it
      node.setAttribute('oncommand', ' ');
      node.addEventListener('command', commands[name]);
      commandset.appendChild(node);
    });
  }
};
