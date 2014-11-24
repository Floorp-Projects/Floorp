      <handler event="keypress" key=" " modifiers="shift" command="cmd_scrollPageUp" />
      <handler event="keypress" key=" " command="cmd_scrollPageDown" />

      <handler event="keypress" keycode="VK_LEFT" command="cmd_moveLeft"/>
      <handler event="keypress" keycode="VK_RIGHT" command="cmd_moveRight"/>
      <handler event="keypress" keycode="VK_LEFT" modifiers="shift" command="cmd_selectLeft"/>
      <handler event="keypress" keycode="VK_RIGHT" modifiers="shift" command="cmd_selectRight"/>

      <handler event="keypress" keycode="VK_UP" command="cmd_moveUp"/>
      <handler event="keypress" keycode="VK_DOWN" command="cmd_moveDown"/>
      <handler event="keypress" keycode="VK_UP" modifiers="shift" command="cmd_selectUp"/>
      <handler event="keypress" keycode="VK_DOWN" modifiers="shift" command="cmd_selectDown"/>

      <handler event="keypress" key="z" command="cmd_undo" modifiers="accel"/>
      <handler event="keypress" key="z" command="cmd_redo" modifiers="accel,shift" />
      <handler event="keypress" key="x" command="cmd_cut" modifiers="accel"/>
      <handler event="keypress" key="c" command="cmd_copy" modifiers="accel"/>
      <handler event="keypress" key="v" command="cmd_paste" modifiers="accel"/>
      <handler event="keypress" key="v" command="cmd_pasteNoFormatting" modifiers="accel,shift"/>
