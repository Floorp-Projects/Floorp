<commandset id="webNavigationItems"
        commandupdater="true"
        events="navigation"
        oncommandupdate="ComposerCommands.goUpdateComposerMenuItems(this)">
  <command id="cmd_stopLoading"  oncommand="goDoCommand('cmd_stopLoading')"/>
</commandset>

<keyset id="editorKeys">
  <key id="key_newNavigator" key="n" modifiers="control" />
</keyset>

