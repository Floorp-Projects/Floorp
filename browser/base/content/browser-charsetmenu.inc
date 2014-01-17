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
#expand    onpopupshowing="CharsetMenu.build(event, '__ID_PREFIX__');"
#else
#expand    onpopupshowing="CharsetMenu.build(event, '__ID_PREFIX__', true);"
#endif
    onpopupshown="UpdateMenus(event);">
  <menupopup>
    <menu label="&charsetMenuAutodet.label;"
#ifndef OMIT_ACCESSKEYS
          accesskey="&charsetMenuAutodet.accesskey;"
#endif
        >
      <menupopup>
        <menuitem type="radio"
                  name="detectorGroup"
#expand           id="__ID_PREFIX__chardet.off"
                  label="&charsetMenuAutodet.off.label;"
#ifndef OMIT_ACCESSKEYS
                  accesskey="&charsetMenuAutodet.off.accesskey;"
#endif
                  />
        <menuitem type="radio"
                  name="detectorGroup"
#expand           id="__ID_PREFIX__chardet.ja_parallel_state_machine"
                  label="&charsetMenuAutodet.ja.label;"
#ifndef OMIT_ACCESSKEYS
                  accesskey="&charsetMenuAutodet.ja.accesskey;"
#endif
                  />
        <menuitem type="radio"
                  name="detectorGroup"
#expand           id="__ID_PREFIX__chardet.ruprob"
                  label="&charsetMenuAutodet.ru.label;"
#ifndef OMIT_ACCESSKEYS
                  accesskey="&charsetMenuAutodet.ru.accesskey;"
#endif
                  />
        <menuitem type="radio"
                  name="detectorGroup"
#expand           id="__ID_PREFIX__chardet.ukprob"
                  label="&charsetMenuAutodet.uk.label;"
#ifndef OMIT_ACCESSKEYS
                  accesskey="&charsetMenuAutodet.uk.accesskey;"
#endif
                  />
      </menupopup>
    </menu>
    <menuseparator/>
  </menupopup>
</menu>
