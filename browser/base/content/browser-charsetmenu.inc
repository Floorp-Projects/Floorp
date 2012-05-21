# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#filter substitution

#expand <menu id="__ID_PREFIX__charsetMenu"
    label="&charsetMenu.label;"
#ifndef OMIT_ACCESSKEYS
    accesskey="&charsetMenu.accesskey;"
#endif
    datasources="rdf:charset-menu"
    ref="NC:BrowserCharsetMenuRoot"
    oncommand="MultiplexHandler(event)"
    onpopupshowing="CreateMenu('browser'); CreateMenu('more-menu');"
    onpopupshown="UpdateMenus(event);"
    observes="isImage">
  <template>
    <rule rdf:type="http://home.netscape.com/NC-rdf#BookmarkSeparator">
      <menupopup>
      <menuseparator uri="..." />
      </menupopup>
    </rule>
    <rule>
      <menupopup>
      <menuitem type="radio" name="charsetGroup" checked="rdf:http://home.netscape.com/NC-rdf#Checked" uri="..." label="rdf:http://home.netscape.com/NC-rdf#Name"/>
      </menupopup>
    </rule>
  </template>

  <menupopup>
  <menu label="&charsetMenuAutodet.label;"
#ifndef OMIT_ACCESSKEYS
        accesskey="&charsetMenuAutodet.accesskey;"
#endif
        datasources="rdf:charset-menu" ref="NC:BrowserAutodetMenuRoot">
    <template>
      <rule rdf:type="http://home.netscape.com/NC-rdf#CharsetDetector">
        <menupopup>
        <menuitem type="radio" name="detectorGroup" checked="rdf:http://home.netscape.com/NC-rdf#Checked" uri="..." label="rdf:http://home.netscape.com/NC-rdf#Name"/>
        </menupopup>
      </rule>
    </template>
    <menupopup>
    </menupopup>
  </menu>
  <menu label="&charsetMenuMore.label;"
#ifndef OMIT_ACCESSKEYS
        accesskey="&charsetMenuMore.accesskey;"
#endif
        datasources="rdf:charset-menu" ref="NC:BrowserMoreCharsetMenuRoot">
    <template>
      <rule>
        <menupopup>
        <menuitem uri="..." label="rdf:http://home.netscape.com/NC-rdf#Name"/>
        </menupopup>
      </rule>
    </template>
    <menupopup>
      <menu label="&charsetMenuMore1.label;"
#ifndef OMIT_ACCESSKEYS
            accesskey="&charsetMenuMore1.accesskey;"
#endif
            datasources="rdf:charset-menu" ref="NC:BrowserMore1CharsetMenuRoot">
        <template>
          <rule>
            <menupopup>
            <menuitem uri="..." label="rdf:http://home.netscape.com/NC-rdf#Name"/>
            </menupopup>
          </rule>
        </template>
        <menupopup>
        </menupopup>
      </menu>
      <menu label="&charsetMenuMore2.label;"
#ifndef OMIT_ACCESSKEYS
            accesskey="&charsetMenuMore2.accesskey;"
#endif
            datasources="rdf:charset-menu" ref="NC:BrowserMore2CharsetMenuRoot">
        <template>
          <rule>
            <menupopup>
            <menuitem uri="..." label="rdf:http://home.netscape.com/NC-rdf#Name"/>
            </menupopup>
          </rule>
        </template>
        <menupopup>
        </menupopup>
      </menu>
      <menu label="&charsetMenuMore3.label;"
#ifndef OMIT_ACCESSKEYS
            accesskey="&charsetMenuMore3.accesskey;"
#endif
            datasources="rdf:charset-menu" ref="NC:BrowserMore3CharsetMenuRoot">
        <template>
          <rule>
            <menupopup>
            <menuitem uri="..." label="rdf:http://home.netscape.com/NC-rdf#Name"/>
            </menupopup>
          </rule>
        </template>
        <menupopup>
        </menupopup>
      </menu>
      <menu label="&charsetMenuMore4.label;"
#ifndef OMIT_ACCESSKEYS
            accesskey="&charsetMenuMore4.accesskey;"
#endif
            datasources="rdf:charset-menu" ref="NC:BrowserMore4CharsetMenuRoot">
        <template>
          <rule>
            <menupopup>
            <menuitem uri="..." label="rdf:http://home.netscape.com/NC-rdf#Name"/>
            </menupopup>
          </rule>
        </template>
        <menupopup>
        </menupopup>
      </menu>
      <menu label="&charsetMenuMore5.label;"
#ifndef OMIT_ACCESSKEYS
            accesskey="&charsetMenuMore5.accesskey;"
#endif
            datasources="rdf:charset-menu" ref="NC:BrowserMore5CharsetMenuRoot">
        <template>
          <rule>
            <menupopup>
            <menuitem uri="..." label="rdf:http://home.netscape.com/NC-rdf#Name"/>
            </menupopup>
          </rule>
        </template>
        <menupopup>
        </menupopup>
      </menu>
      <menu label="&charsetMenuUnicode.label;"
#ifndef OMIT_ACCESSKEYS
            accesskey="&charsetMenuUnicode.accesskey;"
#endif
            datasources="rdf:charset-menu" ref="NC:BrowserUnicodeCharsetMenuRoot">
        <template>
          <rule>
            <menupopup>
            <menuitem uri="..." label="rdf:http://home.netscape.com/NC-rdf#Name"/>
            </menupopup>
          </rule>
        </template>
        <menupopup>
        </menupopup>
      </menu>
      <menuseparator />
    </menupopup>
  </menu>
  <menuitem name="charsetCustomize"
#ifndef OMIT_ACCESSKEYS
            accesskey="&charsetCustomize.accesskey;"
#endif
            label="&charsetCustomize.label;"
            oncommand="window.openDialog('chrome://global/content/customizeCharset.xul', 'PrefWindow', 'chrome,modal=yes,resizable=yes', 'browser');"/>
  </menupopup>
</menu>
