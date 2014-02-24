# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

<menu id="charsetMenu"
    label="&charsetMenu.label;"
#ifndef OMIT_ACCESSKEYS
    accesskey="&charsetMenu.accesskey;"
#endif
    oncommand="BrowserSetForcedCharacterSet(event.target.getAttribute('charset'));"
#ifdef OMIT_ACCESSKEYS
    onpopupshowing="CharsetMenu.build(event.target, false);"
#else
    onpopupshowing="CharsetMenu.build(event.target);"
#endif
    onpopupshown="UpdateCurrentCharset(this);">
  <menupopup>
  </menupopup>
</menu>
