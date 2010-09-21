# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

#filter substitution

#expand <menu id="__ID_PREFIX__charsetMenu"
    label="&charsetMenu.label;"
    accesskey="&charsetMenu.accesskey;"
    datasources="rdf:charset-menu"
    ref="NC:BrowserCharsetMenuRoot"
    oncommand="MultiplexHandler(event)"
    onpopupshowing="CreateMenu('browser');UpdateMenus(event)"
    onpopupshown="CreateMenu('more-menu');"
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
  <menu label="&charsetMenuAutodet.label;" accesskey="&charsetMenuAutodet.accesskey;" datasources="rdf:charset-menu" ref="NC:BrowserAutodetMenuRoot">
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
  <menu label="&charsetMenuMore.label;" accesskey="&charsetMenuMore.accesskey;" datasources="rdf:charset-menu" ref="NC:BrowserMoreCharsetMenuRoot">
    <template>
      <rule>
        <menupopup>
        <menuitem uri="..." label="rdf:http://home.netscape.com/NC-rdf#Name"/>
        </menupopup>
      </rule>
    </template>
    <menupopup>
      <menu label="&charsetMenuMore1.label;" accesskey="&charsetMenuMore1.accesskey;" datasources="rdf:charset-menu" ref="NC:BrowserMore1CharsetMenuRoot">
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
      <menu label="&charsetMenuMore2.label;" accesskey="&charsetMenuMore2.accesskey;" datasources="rdf:charset-menu" ref="NC:BrowserMore2CharsetMenuRoot">
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
      <menu label="&charsetMenuMore3.label;" accesskey="&charsetMenuMore3.accesskey;" datasources="rdf:charset-menu" ref="NC:BrowserMore3CharsetMenuRoot">
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
      <menu label="&charsetMenuMore4.label;" accesskey="&charsetMenuMore4.accesskey;" datasources="rdf:charset-menu" ref="NC:BrowserMore4CharsetMenuRoot">
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
      <menu label="&charsetMenuMore5.label;" accesskey="&charsetMenuMore5.accesskey;" datasources="rdf:charset-menu" ref="NC:BrowserMore5CharsetMenuRoot">
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
      <menu label="&charsetMenuUnicode.label;" accesskey="&charsetMenuUnicode.accesskey;" datasources="rdf:charset-menu" ref="NC:BrowserUnicodeCharsetMenuRoot">
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
  <menuitem name="charsetCustomize" accesskey="&charsetCustomize.accesskey;" label="&charsetCustomize.label;" oncommand="window.openDialog('chrome://global/content/customizeCharset.xul','PrefWindow', 'chrome,modal=yes,resizable=yes', 'browser')"/>
  </menupopup>
</menu>
