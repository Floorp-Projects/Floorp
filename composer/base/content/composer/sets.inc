<broadcasterset id="broadcasterset">
  <command id="cmd_fullScreen"   oncommand="goDoCommand('cmd_fullScreen')"/>
</broadcasterset>

<keyset id="mainKeySet">
  <key id="key_fullScreen" keycode="VK_F11" command="cmd_fullScreen"/>
</keyset>

<commandset id="webNavigationItems"
            commandupdater="true"
            events="navigation"
            oncommandupdate="ComposerCommands.goUpdateComposerMenuItems(this)">
  <command id="cmd_stopLoading"                 oncommand="goDoCommand('cmd_stopLoading')"/>
  <command id="cmd_new"                         oncommand="goDoCommand('cmd_new')"/>
</commandset>

<commandset id="commonEditorMenuItems"
            commandupdater="true"
            events="create"
            oncommandupdate="ComposerCommands.goUpdateComposerMenuItems(this)">
  <command id="cmd_open"         oncommand="goDoCommand('cmd_open')"/>
</commandset>

<commandset id="composerStyleMenuItems"
            commandupdater="true"
            events="create,style,mode_switch"
            oncommandupdate="ComposerCommands.goUpdateComposerMenuItems(this)">
  <command id="cmd_bold"         state="false"     oncommand="ComposerCommands.doStyleUICommand('cmd_bold')"/>
  <command id="cmd_italic"       state="false"     oncommand="ComposerCommands.doStyleUICommand('cmd_italic')"/>
  <command id="cmd_underline"    state="false"     oncommand="ComposerCommands.doStyleUICommand('cmd_underline')"/>
  <command id="cmd_strong"       state="false"     oncommand="ComposerCommands.doStyleUICommand('cmd_strong')"/>
  <command id="cmd_em"           state="false"     oncommand="ComposerCommands.doStyleUICommand('cmd_em')"/>
  <command id="cmd_paragraphState" state=""        oncommand="ComposerCommands.doStatefulCommand('cmd_paragraphState', event.target.value)"/>
  <command id="cmd_renderedHTMLEnabler"/>
  <command id="cmd_ul"           state="false"     oncommand="ComposerCommands.doStyleUICommand('cmd_ul')"/>
  <command id="cmd_ol"           state="false"     oncommand="ComposerCommands.doStyleUICommand('cmd_ol')"/>
</commandset>

<commandset id="selectionBasedCommands"
            commandupdater="true"
            events="select,style,create"
            oncommandupdate="ComposerCommands.updateSelectionBased(this)">
  <command id="cmd_selectionChanged"/>
</commandset>

<keyset id="editorKeys">
  <key id="key_newNavigator" key="n" modifiers="control" />
</keyset>

<popupset id="editorPopupSet">
  <popup id="main-toolbar-context-menu">
    <menuitem oncommand="CustomizeToolbar('MainToolbox');" id="CustomizeMainToolbar" label="&customizeToolbar.label;" accesskey="&customizeToolbar.accesskey;"/>
  </popup>
  <popup id="format-toolbar-context-menu">
    <menuitem oncommand="CustomizeToolbar('FormatToolbox');" id="CustomizeFormatToolbar" label="&customizeToolbar.label;" accesskey="&customizeToolbar.accesskey;"/>
  </popup>
</popupset>