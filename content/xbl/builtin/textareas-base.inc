    <handler event="keypress" keycode="VK_LEFT" command="cmd_charPrevious"/>
    <handler event="keypress" keycode="VK_RIGHT" command="cmd_charNext"/>
    <handler event="keypress" keycode="VK_LEFT" modifiers="shift"
        command="cmd_selectCharPrevious"/>
    <handler event="keypress" keycode="VK_RIGHT" modifiers="shift"
        command="cmd_selectCharNext"/>
    <handler event="keypress" keycode="VK_UP" command="cmd_linePrevious"/>
    <handler event="keypress" keycode="VK_DOWN" command="cmd_lineNext"/>
    <handler event="keypress" keycode="VK_UP" modifiers="shift"
        command="cmd_selectLinePrevious"/>
    <handler event="keypress" keycode="VK_DOWN" modifiers="shift"
        command="cmd_selectLineNext"/>

    <!-- Cut/copy/paste/undo -->
    <handler event="keypress" key="c" modifiers="accel" command="cmd_copy"/>
    <handler event="keypress" key="x" modifiers="accel" command="cmd_cut"/>
    <handler event="keypress" key="v" modifiers="accel" command="cmd_paste"/>
    <handler event="keypress" key="z" modifiers="accel" command="cmd_undo"/>
    <handler event="keypress" key="z" command="cmd_redo" modifiers="accel,shift" />
