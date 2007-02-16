  <toolbarpalette id="FormatToolbarPalette">

      <toolbarbutton id="boldButton" type="checkbox" autoCheck="false" observes="cmd_bold" label="&boldToolbarCmd.label;"
                     tooltiptext="&boldToolbarCmd.tooltip;">
        <observes element="cmd_bold" type="checkbox" attribute="state" onbroadcast="onButtonUpdate(this.parentNode, 'cmd_bold')"/>
      </toolbarbutton>

      <toolbarbutton id="italicButton" type="checkbox" autoCheck="false" observes="cmd_italic" label="&italicToolbarCmd.label;"
                     tooltiptext="&italicToolbarCmd.tooltip;">
        <observes element="cmd_italic" attribute="state" onbroadcast="onButtonUpdate(this.parentNode, 'cmd_italic')"/>
      </toolbarbutton>

      <toolbarbutton id="underlineButton" type="checkbox" autoCheck="false" observes="cmd_underline" label="&underlineToolbarCmd.label;"
                     tooltiptext="&underlineToolbarCmd.tooltip;">
        <observes element="cmd_underline" attribute="state" onbroadcast="onButtonUpdate(this.parentNode, 'cmd_underline')"/>
      </toolbarbutton>
    
      <toolbarbutton id="strongButton" type="checkbox" autoCheck="false" observes="cmd_strong" label="&strongToolbarCmd.label;"
                     tooltiptext="&strongToolbarCmd.tooltip;">
        <observes element="cmd_strong" attribute="state" onbroadcast="onButtonUpdate(this.parentNode, 'cmd_strong')"/>
      </toolbarbutton>
    
      <toolbarbutton id="emButton" type="checkbox" autoCheck="false" observes="cmd_em" label="&emToolbarCmd.label;"
                     tooltiptext="&emToolbarCmd.tooltip;">
        <observes element="cmd_em" attribute="state" onbroadcast="onButtonUpdate(this.parentNode, 'cmd_em')"/>
      </toolbarbutton>

      <toolbaritem id="ParagraphSelectBox">
        <menulist class="toolbar-focustarget" id="ParagraphSelect" observes="cmd_renderedHTMLEnabler"
          tooltiptext="&ParagraphSelect.tooltip;" crop="right">
          <observes element="cmd_paragraphState" attribute="state" onbroadcast="onParagraphFormatChange(this.parentNode, 'cmd_paragraphState')"/>
          <menupopup id="ParagraphPopup" oncommand="ComposerCommands.doStatefulCommand('cmd_paragraphState', event.target.value)">
            <!-- TODO : add an onpopupshowing handler to hide the body text
                        choice in strict mode -->
            <menuitem label="&bodyTextCmd.label;"            value=""/>
            <menuitem label="&paragraphParagraphCmd.label;"  value="p"/>
            <menuitem label="&heading1Cmd.label;"            value="h1"/>
            <menuitem label="&heading2Cmd.label;"            value="h2"/>
            <menuitem label="&heading3Cmd.label;"            value="h3"/>
            <menuitem label="&heading4Cmd.label;"            value="h4"/>
            <menuitem label="&heading5Cmd.label;"            value="h5"/>
            <menuitem label="&heading6Cmd.label;"            value="h6"/>
            <menuitem label="&paragraphAddressCmd.label;"    value="address"/>
            <menuitem label="&paragraphPreformatCmd.label;"  value="pre"/>
            <menuseparator />
            <menuitem label="&genericContainerCmd.label;"    value="div"/>
          </menupopup>
        </menulist>
      </toolbaritem>

    <toolbarbutton id="ulButton" type="radio" group="lists" autoCheck="false" observes="cmd_ul"
                   tooltiptext="&unorderedList.tooltip;">
        <observes element="cmd_ul" attribute="state" onbroadcast="onButtonUpdate(this.parentNode, 'cmd_ul')"/>
    </toolbarbutton>

    <toolbarbutton id="olButton" type="radio" group="lists" autoCheck="false" observes="cmd_ol"
                   tooltiptext="&orderedList.tooltip;">
        <observes element="cmd_ol" attribute="state" onbroadcast="onButtonUpdate(this.parentNode, 'cmd_ol')"/>
    </toolbarbutton>

    </toolbarpalette>
