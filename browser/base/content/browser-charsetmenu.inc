# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#filter substitution

#expand <menu id="__ID_PREFIX__charsetMenu"
    label="&charsetMenu.label;"
#ifndef OMIT_ACCESSKEYS
    accesskey="&charsetMenu.accesskey;"
#endif
    oncommand="MultiplexHandler(event)"
#ifdef OMIT_ACCESSKEYS
#expand    onpopupshowing="CharsetMenu.build(event.target, '__ID_PREFIX__', false);"
#else
#expand    onpopupshowing="CharsetMenu.build(event.target, '__ID_PREFIX__');"
#endif
    onpopupshown="UpdateMenus(event);">
  <menupopup>
  </menupopup>
</menu>
