    <popup id="placesContext" 
           onpopupshowing="return PlacesController.buildContextMenu(this);">
      <menuitem id="placesContext_open" command="placesCmd_open" default="true"
                selection="link"/>
      <menuitem id="placesContext_open:tabs" command="placesCmd_open:tabs"
                selection="folder|links"/>
      <menuitem id="placesContext_open:newwindow" command="placesCmd_open:window"
                selection="link"/>
      <menuitem id="placesContext_open:newtab" command="placesCmd_open:tab"
                selection="link"/>
      <menuseparator id="placesContext_openSeparator"
                selection="link|links|folder"/>
      <menuitem id="placesContext_new:folder" command="placesCmd_new:folder"
                selection="mutable"/>
      <menuitem id="placesContext_new:separator" command="placesCmd_new:separator"
                selection="mutable"/>
      <menuseparator id="placesContext_newSeparator"
                selection="mutable"/>
      <menuitem id="placesContext_edit:cut" command="placesCmd_edit:cut"
                selection="mixed"/>
      <menuitem id="placesContext_edit:copy" command="placesCmd_edit:copy"
                selection="mixed"/>
      <menuitem id="placesContext_edit:paste" command="placesCmd_edit:paste"
                selection="mixed"/>
      <menuitem id="placesContext_edit:delete" command="placesCmd_edit:delete"
                selection="mixed"/>
      <menuseparator id="placesContext_editSeparator"
                selection="mixed"/>
      <menuitem id="placesContext_select:all" command="placesCmd_select:all"
                selection="multiselect"/>
      <menuseparator id="placesContext_selectSeparator"
                selection="multiselect"/>
      <menuitem id="placesContext_reload" command="placesCmd_reload"
                selection="livemark"/>
      <menuitem id="placesContext_sortby:name" command="placesCmd_sortby:name"
                selection="mutable"/>
      <menuseparator id="placesContext_sortSeparator"
                selection="mutable|livemark"/>
      <menuitem id="placesContext_show:info" command="placesCmd_show:info"
                selection="link|folder"/>
    </popup>
