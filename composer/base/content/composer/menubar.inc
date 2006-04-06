  <menubar id="composer-main-menubar" class="chromeclass-menubar">
    <menu id="fileMenu" label="&fileMenu.label;" accesskey="&fileMenu.accesskey;">
      <menupopup id="fileMenu_popup">
        <menuitem label="&newMenu.label;" accesskey="&newMenu.accesskey;" key="key_newNavigator"
                  oncommand="OpenLocation()"/>
        <menuseparator/>
        <menuitem label="&quit.label;" accesskey="&quit.accesskey;" oncommand="goQuitApplication()"/>
      </menupopup>
    </menu>
    <menu id="editMenu"   label="&editMenu.label;"   accesskey="&editMenu.accesskey;">
      <menupopup/>
    </menu>
    <menu id="viewMenu"   label="&viewMenu.label;"   accesskey="&viewMenu.accesskey;">
      <menupopup>
        <menuitem label="Customize toolbar" oncommand="CustomizeToolbar('MainToolbox')"/>
      </menupopup>
    </menu>
    <menu id="insertMenu" label="&insertMenu.label;" accesskey="&insertMenu.accesskey;">
      <menupopup/>
    </menu>
    <menu id="formatMenu" label="&formatMenu.label;" accesskey="&formatMenu.accesskey;">
      <menupopup/>
    </menu>
    <menu id="tableMenu" label="&tableMenu.label;" accesskey="&tableMenu.accesskey;">
      <menupopup/>
    </menu>
    <menu id="toolsMenu" label="&toolsMenu.label;" accesskey="&toolsMenu.accesskey;">
      <menupopup id="toolsPopup">
        <menuitem label="Extensions"  oncommand="OpenExtensionsManager()"/>
        <menuseparator/>
        <menuitem label="&consoleMenu.label;"  oncommand="OpenConsole()"/>
      </menupopup>
    </menu>
    <menu id="helpMenu" label="&helpMenu.label;" accesskey="&helpMenu.accesskey;">
      <menupopup>
        <menuitem label="&aboutComposer.label;" oncommand=""/>
      </menupopup>
    </menu>
  </menubar>
