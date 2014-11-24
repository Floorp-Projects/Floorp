    <handler event="keypress" keycode="VK_LEFT" command="cmd_moveLeft"/>
    <handler event="keypress" keycode="VK_RIGHT" command="cmd_moveRight"/>
    <handler event="keypress" keycode="VK_LEFT" modifiers="shift" command="cmd_selectLeft"/>
    <handler event="keypress" keycode="VK_RIGHT" modifiers="shift" command="cmd_selectRight"/>

    <handler event="keypress" keycode="VK_UP" command="cmd_moveUp"/>
    <handler event="keypress" keycode="VK_DOWN" command="cmd_moveDown"/>
    <handler event="keypress" keycode="VK_UP" modifiers="shift" command="cmd_selectUp"/>
    <handler event="keypress" keycode="VK_DOWN" modifiers="shift" command="cmd_selectDown"/>

    <!-- Cut/copy/paste/undo -->
    <handler event="keypress" key="c" modifiers="accel" command="cmd_copy"/>
    <handler event="keypress" key="x" modifiers="accel" command="cmd_cut"/>
    <handler event="keypress" key="v" modifiers="accel" command="cmd_paste"/>
    <handler event="keypress" key="z" modifiers="accel" command="cmd_undo"/>
    <handler event="keypress" key="z" modifiers="accel,shift" command="cmd_redo" />

