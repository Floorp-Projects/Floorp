# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#filter substitution

#expand <menu id="__ID_PREFIX__charsetMenu"
    label="&charsetMenu.label;"
    accesskey="&charsetMenu.accesskey;"
    oncommand="MultiplexHandler(event)"
    onpopupshowing="CharsetMenu.build(event);"
    onpopupshown="UpdateMenus(event);">
  <menupopup>
    <menu label="&charsetMenuAutodet.label;"
          accesskey="&charsetMenuAutodet.accesskey;">
      <menupopup>
        <menuitem type="radio"
                  name="detectorGroup"
                  id="chardet.off"
                  label="&charsetMenuAutodet.off.label;"
                  accesskey="&charsetMenuAutodet.off.accesskey;"/>
        <menuitem type="radio"
                  name="detectorGroup"
                  id="chardet.ja_parallel_state_machine"
                  label="&charsetMenuAutodet.ja.label;"
                  accesskey="&charsetMenuAutodet.ja.accesskey;"/>
        <menuitem type="radio"
                  name="detectorGroup"
                  id="chardet.ruprob"
                  label="&charsetMenuAutodet.ru.label;"
                  accesskey="&charsetMenuAutodet.ru.accesskey;"/>
        <menuitem type="radio"
                  name="detectorGroup"
                  id="chardet.ukprob"
                  label="&charsetMenuAutodet.uk.label;"
                  accesskey="&charsetMenuAutodet.uk.accesskey;"/>
      </menupopup>
    </menu>
    <menuseparator/>
  </menupopup>
</menu>
