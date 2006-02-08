  <commandset id="placesCommands" 
              commandupdater="true" events="focus,select,mousedown" 
              oncommandupdate="PlacesController.onCommandUpdate()">
    <command id="placesCmd_find" label="&cmd.find.label;" accesskey="&cmd.find.accesskey;"
             oncommand="PlacesSearchBox.focus();"/>
    <command id="placesCmd_export" label="&cmd.export.label;" accesskey="&cmd.export.accesskey;"/>
    <command id="placesCmd_import"/>
    <command id="placesCmd_search:save"/>
    <command id="placesCmd_search:moreCriteria"
             oncommand="PlacesQueryBuilder.toggle();"/>
    <command id="placesCmd_select:all" 
             label="&cmd.select_all.label;" accesskey="&cmd.select_all.accesskey;"
             oncommand="PlacesController.selectAll();"/>
    <commandset readonly="true">
      <command id="placesCmd_edit:copy" 
               label="&cmd.edit_copy.label;" accesskey="&cmd.edit_copy.accesskey;"
               oncommand="PlacesController.copy();"/>
      <command id="placesCmd_show:info" 
#ifdef XP_WIN
               label="&cmd.show_infoWin.label;" accesskey="&cmd.show_infoWin.accesskey;"/>
#else
               label="&cmd.show_infoMac.label;" accesskey="&cmd.show_infoMac.accesskey;"/>
#endif
    </commandset>
    <commandset>
      <command id="placesCmd_edit:cut" 
               label="&cmd.edit_cut.label;" accesskey="&cmd.edit_cut.accesskey;"
               oncommand="PlacesController.cut();"/>
      <command id="placesCmd_edit:paste" 
               label="&cmd.edit_paste.label;" accesskey="&cmd.edit_paste.accesskey;"
               oncommand="PlacesController.paste();"/>
      <command id="placesCmd_edit:delete" 
               label="&cmd.edit_delete.label;" accesskey="&cmd.edit_delete.accesskey;"
               oncommand="PlacesController.remove('Remove Selection');"/>
      <command id="placesCmd_rename" label="&cmd.rename.label;" accesskey="&cmd.rename.accesskey;"/>
    </commandset>
    <commandset type="link" readonly="true">
      <command id="placesCmd_open" label="&cmd.open.label;" accesskey="&cmd.open.accesskey;"
               oncommand="PlacesController.openLinkInCurrentWindow();"/>
      <command id="placesCmd_open:window" label="&cmd.open_window.label;" accesskey="&cmd.open_window.accesskey;"
               oncommand="PlacesController.openLinkInNewWindow();"/>
      <command id="placesCmd_open:tab" label="&cmd.open_tab.label;" accesskey="&cmd.open_tab.accesskey;"
               oncommand="PlacesController.openLinkInNewTab();"/>
    </commandset>
    <commandset type="link">
    </commandset>
    <commandset type="container" readonly="true">
      <command id="placesCmd_open:tabs" label="&cmd.open_tabs.label;" accesskey="&cmd.open_tabs.accesskey;"
               oncommand="PlacesController.openLinksInTabs();"/>
      <command id="placesCmd_groupby:site" 
               label="&cmd.groupby_site.label;" accesskey="&cmd.groupby_site.accesskey;"
               oncommand="PlacesController.groupBySite();"/>
      <command id="placesCmd_groupby:page" 
               label="&cmd.groupby_page.label;" accesskey="&cmd.groupby_page.accesskey;"
               oncommand="PlacesController.groupByPage();"/>
      <command id="placesCmd_groupby:feed" label="&cmd.groupby_feed.label;" accesskey="&cmd.groupby_feed.accesskey;"/>
      <command id="placesCmd_groupby:post" label="&cmd.groupby_post.label;" accesskey="&cmd.groupby_post.accesskey;"/>
    </commandset>
    <commandset type="container">
      <command id="placesCmd_sortby:name" label="&cmd.sortby_name.label;" accesskey="&cmd.sortby_name.accesskey;"/>
      <command id="placesCmd_new:folder" 
               label="&cmd.new_folder.label;" accesskey="&cmd.new_folder.accesskey;"
               oncommand="PlacesController.newFolder()"/>
      <command id="placesCmd_new:separator" label="&cmd.new_separator.label;" accesskey="&cmd.new_separator.accesskey;"
               oncommand="PlacesController.newSeparator()"/>
    </commandset>
    <commandset type="container|feed" readonly="true">
      <command id="placesCmd_reload" label="&cmd.reload.label;" accesskey="&cmd.reload.accesskey;"/>
    </commandset>
  </commandset>
