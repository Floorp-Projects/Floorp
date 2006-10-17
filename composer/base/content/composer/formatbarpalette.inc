  <toolbarpalette id="FormatToolbarPalette">

      <toolbarbutton id="boldButton" type="checkbox" autoCheck="false" observes="cmd_bold"
                     tooltiptext="&boldToolbarCmd.tooltip;">
        <observes element="cmd_bold" type="checkbox" attribute="state" onbroadcast="onButtonUpdate(this.parentNode, 'cmd_bold')"/>
      </toolbarbutton>

      <toolbarbutton id="italicButton" type="checkbox" autoCheck="false" observes="cmd_italic" label="italic"
                     tooltiptext="&italicToolbarCmd.tooltip;">
        <observes element="cmd_italic" attribute="state" onbroadcast="onButtonUpdate(this.parentNode, 'cmd_italic')"/>
      </toolbarbutton>

      <toolbarbutton id="underlineButton" type="checkbox" autoCheck="false" observes="cmd_underline"
                     tooltiptext="&underlineToolbarCmd.tooltip;">
        <observes element="cmd_underline" attribute="state" onbroadcast="onButtonUpdate(this.parentNode, 'cmd_underline')"/>
      </toolbarbutton>
    
    </toolbarpalette>
