      <handler event="draggesture">
<!--
         nsDragAndDrop.startDrag(event, contentAreaDNDObserver);
-->
      </handler>
      <handler event="dragover">
<!--
         nsDragAndDrop.dragOver(event, contentAreaDNDObserver);
-->
      </handler>
      <handler event="dragdrop">
<!--
         nsDragAndDrop.drop(event, contentAreaDNDObserver);
-->
      </handler>

      <handler event="keypress" key=" " modifiers="shift" command="cmd_scrollPageUp" />
      <handler event="keypress" key=" " command="cmd_scrollPageDown" />

      <handler event="keypress" keycode="VK_UP" command="cmd_scrollLineUp" />
      <handler event="keypress" keycode="VK_DOWN" command="cmd_scrollLineDown" />
      <handler event="keypress" keycode="VK_LEFT" command="cmd_scrollLeft" />
      <handler event="keypress" keycode="VK_RIGHT" command="cmd_scrollRight" />

      <handler event="keypress" keycode="VK_HOME" command="cmd_scrollTop"/>
      <handler event="keypress" keycode="VK_END" command="cmd_scrollBottom"/>

      <handler event="keypress" key="x" command="cmd_cut" modifiers="accel"/>
      <handler event="keypress" key="c" command="cmd_copy" modifiers="accel"/>
      <handler event="keypress" key="v" command="cmd_paste" modifiers="accel"/>
      <handler event="keypress" key="z" command="cmd_undo" modifiers="accel"/>
      <handler event="keypress" key="z" command="cmd_redo" modifiers="accel,shift" />
      <handler event="keypress" key="a" command="cmd_selectAll" modifiers="accel"/>

      <handler event="keypress" key="/" command="cmd_findTypeText"/>
      <handler event="keypress" key="'" command="cmd_findTypeLinks"/>
