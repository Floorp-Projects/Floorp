      <handler event="keypress" keycode="VK_LEFT" command="cmd_charPrevious"/>
      <handler event="keypress" keycode="VK_RIGHT" command="cmd_charNext"/>
      <handler event="keypress" keycode="VK_LEFT" command="cmd_selectCharPrevious" modifiers="shift"/>
      <handler event="keypress" keycode="VK_RIGHT" command="cmd_selectCharNext" modifiers="shift"/>
      <handler event="keypress" keycode="VK_UP" command="cmd_linePrevious"/>
      <handler event="keypress" keycode="VK_DOWN" command="cmd_lineNext"/>
      <handler event="keypress" keycode="VK_UP" command="cmd_selectLinePrevious" modifiers="shift"/>
      <handler event="keypress" keycode="VK_DOWN" command="cmd_selectLineNext" modifiers="shift"/>
      <handler event="keypress" key="z" command="cmd_undo" modifiers="accel"/>
      <handler event="keypress" key="z" command="cmd_redo" modifiers="accel,shift" />
      <handler event="keypress" key="x" command="cmd_cut" modifiers="accel"/>
      <handler event="keypress" key="c" command="cmd_copy" modifiers="accel"/>
      <handler event="keypress" key="v" command="cmd_paste" modifiers="accel"/>
