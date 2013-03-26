<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<vbox id="customization-container" flex="1" style="background: #DBEAF9;">
  <hbox flex="1">
    <richlistbox id="customization-categories"
                 onselect="document.getElementById('customization-content').selectedIndex = this.selectedIndex;">
      <richlistitem>
        <label value="&customizeMode.menuAndToolbars.label;"/>
        </richlistitem>
        <richlistitem>
        <label value="&customizeMode.appearance.label;"/>
      </richlistitem>
    </richlistbox>
    <deck id="customization-content" flex="1">
      <hbox id="customization-widgets">
        <vbox flex="1">
          <label value="&customizeMode.menuAndToolbars.header;"/>
          <vbox id="customization-palette" flex="1" style="display: block;"/>
        </vbox>
        <vbox>
          <hbox id="customization-panelHolder"/>
          <spacer flex="1"/>
        </vbox>
      </hbox>
      <vbox id="customization-appearance">
        <label value="&customizeMode.appearance.header;"/>
      </vbox>
    </deck>
  </hbox>
  <hbox pack="end">
    <button oncommand="gCustomizeMode.reset();" label="&customizeMode.restoreDefaults;"/>
    <button oncommand="BrowserToolboxCustomizeDone();" label="&customizeMode.done;"/>
  </hbox>
</vbox>
